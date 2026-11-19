/*
 * The touch screen of the OnePlus 8T (kebab, KB2000, SM8250): a Synaptics S3908 at I2C address 0x4B, as a
 * source of touch records for /dev/hk-touch (sys/hk_input.h). On for board=kebab; kebab_tcm=0 turns it off.
 *
 * Why not the OnePlus 8's S6SY761: the 8T's own device tree (its live DTB, read 2026-11-17) carries three touch
 * nodes on the same bus, i2c@a94000 -- sec-s6sy761@48, synaptics-s3908@4B and st_fts@49 -- because the dtsi is
 * shared by several boards; kebab's own file (kebab-oem.dtsi in the vendor kernel) defines only the S3908, and
 * the phone's Android says androidboot.project_name=19805, the project-name of that node, whose "panel" list is
 * the only one naming the panel this phone runs (qcom,mdss_dsi_samsung_amb655x_dsc_cmd). A run on the phone
 * settled it (2026-11-18): with both lines idling high and the supplies up, the only address that answered was
 * 0x4b: "tl: touch: bus idle sda 1 scl 1; answering: 0x4b" (instantnoodle_touch.c).
 *
 * Bus and pins are the 8's: serial engine 13 (i2c@a94000) with gpio36 data and gpio37 clock, external pull-ups;
 * reset gpio38 (active low), report line gpio39 (active low). This driver does not use the engine either but
 * drives the two pins (instantnoodle_i2c.c; open drain). Supplies: 1.8 V I/O from PM8150A LDO1 ("ldoc1") and
 * 3.008 V analog from PM8150 LDO13 ("ldoa13") through RPMh, as the 8T's device tree has them (pm8150a_l1 and
 * pm8150_l13 at 3008000 uV). Coordinates: touchpanel,panel-coords <1079 2399>, scaled to 0..65535.
 *
 * Protocol: Synaptics TouchComm (TCM), the one the vendor's android_kernel_oneplus_sm8250 drives with
 * drivers/input/touchscreen/synaptics_tcm (read for this driver: its synaptics_tcm_core.h, _core.c and
 * _touch.c). There are no register addresses on the wire:
 *
 *   command (I2C write):  [command] [length lo] [length hi] [payload ...]
 *   message (I2C read):   [0xa5] [code] [length lo] [length hi] [payload ...] [0x5a]
 *
 * A read takes the message from its start; if the first read was short the next one continues where it stopped
 * (the vendor's "continued read"). A message whose code is 0x00..0x0f is a status -- the answer to a command
 * (0x01 ok, 0x00 idle, 0x02 busy, ...); anything above is a report, 0x11 a touch report. While no message waits
 * a read returns the idle status, which is why the vendor can poll instead of waiting for the report line
 * (do_polling). This driver watches gpio39 and reads anyway every 20 ms, so a missed edge cannot stop the screen.
 *
 * Start-up: hold reset, bring the supplies up, let reset go, wait, identify (0x02: version, mode, part number,
 * build id); if the chip came up in its bootloader (0x0b/0x0c), ask it to run the application firmware (0x14)
 * and identify again; ask for the application info (0x20: max_x, max_y, max_objects at offsets 32, 34, 36) and
 * for the touch report configuration (0x25). Reports are parsed according to that configuration -- a small
 * program over a bit stream (FOREACH_ACTIVE_OBJECT ... END), so nothing about the report's layout is assumed
 * here: the chip's own answer is what is walked. A finger the report does not mention has lifted (the objects
 * are cleared each frame and compared with the last).
 */
#include <kern/clock.h>
#include <kern/sched_prim.h>
#include <kern/thread.h>
#include <libkern/libkern.h>
#include <mach/mach_types.h>
#include <pexpert/pexpert.h>
#include <sys/errno.h>
#include <sys/instantnoodle.h>
#include <sys/hk_input.h>
#include <sys/systm.h>

extern void hk_tl(const char *format, ...) __printflike(1, 2);
extern kern_return_t thread_terminate(thread_t thread);
/* (declared here as instantnoodle_touch.c does: the build's copy of kern/thread.h does not give it to bsd/) */
extern kern_return_t kernel_thread_start_priority(thread_continue_t continuation, void *parameter, integer_t priority,
    thread_t *new_thread);

static void
sleep_ms(uint32_t ms)
{
	delay_for_interval(ms, NSEC_PER_MSEC);
}

/* ---- pins and the bus (sys/instantnoodle.h) --------------------------------------------------- */

#define PIN(n)                  ((struct instantnoodle_pin){ INSTANTNOODLE_TLMM_SOUTH, (n) })
#define PIN_RESET               PIN(38)
#define PIN_IRQ                 PIN(39)

static struct instantnoodle_i2c bus = { .name = "se13", .sda = PIN(36), .scl = PIN(37) };

/* ---- the TCM protocol ------------------------------------------------------------------------ */

#define TCM_ADDR                0x4b
#define TCM_PADDING             0x5a
#define TCM_MARKER              0xa5
#define TCM_HEADER              4
#define TCM_MIN_READ            9       /* the vendor's MIN_READ_LENGTH: a header, and room for a short answer */
#define TCM_MAX                 280

#define CMD_IDENTIFY            0x02
#define CMD_RUN_APP_FIRMWARE    0x14
#define CMD_GET_APP_INFO        0x20
#define CMD_GET_TOUCH_CONFIG    0x25

#define STATUS_IDLE             0x00
#define STATUS_OK               0x01
#define STATUS_BUSY             0x02
#define STATUS_CONTINUED_READ   0x03

#define REPORT_IDENTIFY         0x10    /* the answer to CMD_IDENTIFY: a report, not a status (syna_tcm_dispatch_message) */
#define REPORT_TOUCH            0x11
#define REPORT_STATUS           0xc0
#define REPORT_PRINTF           0x82

#define MODE_APPLICATION        0x01

#define MAX_OBJECTS             10      /* the 8T's touchpanel,max-num-support */

enum touch_report_code {
	TOUCH_END = 0,
	TOUCH_FOREACH_ACTIVE_OBJECT,
	TOUCH_FOREACH_OBJECT,
	TOUCH_FOREACH_END,
	TOUCH_PAD_TO_NEXT_BYTE,
	TOUCH_TIMESTAMP,
	TOUCH_OBJECT_N_INDEX,
	TOUCH_OBJECT_N_CLASSIFICATION,
	TOUCH_OBJECT_N_X_POSITION,
	TOUCH_OBJECT_N_Y_POSITION,
	TOUCH_OBJECT_N_Z,
	TOUCH_OBJECT_N_X_WIDTH,
	TOUCH_OBJECT_N_Y_WIDTH,
	TOUCH_OBJECT_N_TX_POSITION_TIXELS,
	TOUCH_OBJECT_N_RX_POSITION_TIXELS,
	TOUCH_0D_BUTTONS_STATE,
	TOUCH_GESTURE_DOUBLE_TAP,
	TOUCH_FRAME_RATE,
	TOUCH_POWER_IM,
	TOUCH_CID_IM,
	TOUCH_RAIL_IM,
	TOUCH_CID_VARIANCE_IM,
	TOUCH_NSM_FREQUENCY,
	TOUCH_NSM_STATE,
	TOUCH_NUM_OF_ACTIVE_OBJECTS,
	TOUCH_NUM_OF_CPU_CYCLES_USED_SINCE_LAST_FRAME,
	TOUCH_TUNING_GAUSSIAN_WIDTHS = 0x80,
	TOUCH_TUNING_SMALL_OBJECT_PARAMS,
	TOUCH_TUNING_0D_BUTTONS_VARIANCE,
};

static uint8_t in_buf[TCM_MAX], out_buf[TCM_MAX];
static uint8_t config[128];
static unsigned config_length;
static unsigned max_objects = MAX_OBJECTS, panel_w = 1080, panel_h = 2400;

static struct {
	uint8_t status;                 /* this frame: the classification the report gave it, 0 for "not in it" */
	uint32_t x, y;
} object[MAX_OBJECTS];
static struct {
	uint8_t status;                 /* the frame before: what was posted */
	uint32_t x, y;                  /* where it was posted */
} posted[MAX_OBJECTS];

static unsigned frames, bus_errors, out_of_sync, read_errors;
static unsigned posts_down, posts_move, posts_up;        /* what post_frame() has sent, for the summary line */

static int
tcm_read(uint8_t *buf, unsigned n)
{
	return instantnoodle_i2c_recv(&bus, TCM_ADDR, buf, n);
}

/* The first few bad reads as bytes: whether the bus is quiet (0xff), pulled low (0x00), or something else. */
static void
log_read(const char *what, const uint8_t *buf, unsigned n)
{
	char line[96];
	unsigned at = 0;

	at += snprintf(line, sizeof(line), "tl: tcm: %s (attn %d):", what, instantnoodle_gpio_level(PIN_IRQ));
	for (unsigned i = 0; i < n && at < sizeof(line) - 4; i++) {
		at += snprintf(line + at, sizeof(line) - at, " %02x", buf[i]);
	}
	hk_tl("%s\n", line);
}

/*
 * The next message: its code and payload. 0, or EIO when the chip answered nothing this time, or EMSGSIZE.
 *
 * The first transfer takes TCM_MIN_READ bytes: the header and the start of the payload. What follows comes in
 * chunks of [0xa5][0x03][payload], one header per chunk -- the vendor's "continued read" (syna_tcm_continued_read:
 * it reads xfer_length + 2 bytes and drops the two), so the payload is moved back over them here. A message whose
 * last byte is the only one left is not read at all: it is padding (the vendor writes MESSAGE_PADDING itself).
 */
static int
tcm_message(uint8_t *code, uint8_t *payload, unsigned *length)
{
	unsigned total, len, remaining;

	if (tcm_read(in_buf, TCM_MIN_READ) != 0) {
		read_errors++;
		if (read_errors <= 4) {
			hk_tl("tl: tcm: read failed (attn %d)\n", instantnoodle_gpio_level(PIN_IRQ));
		}
		return EIO;
	}
	if (in_buf[0] != TCM_MARKER) {
		bus_errors++;
		if (bus_errors <= 6) {
			log_read("read: no marker", in_buf, TCM_MIN_READ);
		}
		return EIO;
	}
	*code = in_buf[1];
	len = (unsigned)in_buf[2] | (unsigned)in_buf[3] << 8;
	total = TCM_HEADER + len + 1;
	if (total + 2 > sizeof(in_buf)) {
		return EMSGSIZE;
	}
	remaining = total > TCM_MIN_READ ? total - TCM_MIN_READ : 0;
	if (remaining == 1) {
		in_buf[total - 1] = TCM_PADDING;
	} else if (remaining > 1) {
		if (tcm_read(in_buf + TCM_MIN_READ, remaining + 2) != 0) {
			read_errors++;
			return EIO;
		}
		if (in_buf[TCM_MIN_READ] != TCM_MARKER || in_buf[TCM_MIN_READ + 1] != STATUS_CONTINUED_READ) {
			bus_errors++;
			if (bus_errors <= 6) {
				log_read("read: continuation", in_buf + TCM_MIN_READ, remaining + 2);
			}
			return EIO;
		}
		for (unsigned i = 0; i < remaining; i++) {
			in_buf[TCM_MIN_READ + i] = in_buf[TCM_MIN_READ + 2 + i];
		}
	}
	if (in_buf[total - 1] != TCM_PADDING) {
		out_of_sync++;
		if (out_of_sync <= 4) {
			log_read("read: no padding", in_buf, total);
		}
	}
	if (payload != NULL) {
		memcpy(payload, in_buf + TCM_HEADER, len);
	}
	*length = len;
	return 0;
}

static void parse_touch(const uint8_t *report, unsigned length);
static void post_frame(void);

/*
 * A command and the answer to it. STATUS_IDLE/BUSY mean "not yet"; a touch report that arrives first is posted
 * rather than dropped. Most commands are answered with STATUS_OK and their payload; CMD_IDENTIFY is answered
 * with a REPORT_IDENTIFY (0x10) whose payload is the identification (syna_tcm_dispatch_message: report codes
 * below REPORT_IDENTIFY are responses, REPORT_IDENTIFY is the answer to the command that asked for it), so
 * `report_answer` names that code for the caller; 0 means the answer is a status. The payload is copied out.
 */
static int
tcm_command(uint8_t command, const uint8_t *payload, unsigned length, uint8_t *answer, unsigned *answer_length,
    unsigned answer_max, uint8_t report_answer)
{
	static uint8_t buf[TCM_MAX];
	uint8_t code;
	unsigned len, waited = 0;
	int err;

	if (length + 3 > sizeof(out_buf)) {
		return EMSGSIZE;
	}
	out_buf[0] = command;
	out_buf[1] = (uint8_t)length;
	out_buf[2] = (uint8_t)(length >> 8);
	if (length != 0) {
		memcpy(out_buf + 3, payload, length);
	}
	if (instantnoodle_i2c_write(&bus, TCM_ADDR, out_buf, length + 3) != 0) {
		bus_errors++;
		return EIO;
	}
	for (;;) {
		err = tcm_message(&code, buf, &len);
		if (err == 0) {
			if (code == STATUS_OK || (report_answer != 0 && code == report_answer)) {
				if (answer != NULL && len != 0) {
					if (len > answer_max) {
						return EMSGSIZE;
					}
					memcpy(answer, buf, len);
				}
				if (answer_length != NULL) {
					*answer_length = len;
				}
				return 0;
			}
			if (code == STATUS_IDLE || code == STATUS_BUSY || code == STATUS_CONTINUED_READ || code == 0xff) {
				/* nothing yet */
			} else if (code <= 0x0f) {
				return EBUSY;           /* the command failed in the chip; its status is the error */
			} else if (code == REPORT_TOUCH) {
				parse_touch(buf, len);
				post_frame();
			} else if (code == REPORT_IDENTIFY) {
				/* the chip announcing that it reset itself; the command's own answer is still to come */
			}
		}
		if (waited >= 1000) {
			hk_tl("tl: tcm: command %#x: no answer in %u ms (%u unreadable, %u bad headers)\n", command, waited,
			    read_errors, bus_errors);
			return ETIMEDOUT;
		}
		sleep_ms(1);
		waited++;
	}
}

/* ---- the reports ----------------------------------------------------------------------------- */

static uint32_t
bits_at(const uint8_t *buf, unsigned length, unsigned offset, unsigned bits)
{
	uint32_t value = 0;
	unsigned byte, bit;

	if (bits == 0 || bits > 32 || offset + bits > length * 8) {
		return 0;
	}
	for (unsigned i = 0; i < bits; i++) {
		byte = (offset + i) / 8;
		bit = (offset + i) % 8;
		value |= (uint32_t)(buf[byte] >> bit & 1) << i;
	}
	return value;
}

/*
 * The frame, walked the way the vendor's touch_parse_report() walks it: the configuration is a small program over
 * a bit stream (each object's fields, repeated for every object or for every active one), and only what this
 * driver needs is kept -- each object's classification and position.
 */
#define CONFIG_BITS(bits)       do {                    \
		if (idx >= config_length) {                 \
			return;                             \
		}                                           \
		(bits) = config[idx++];                     \
	} while (0)

static void
parse_touch(const uint8_t *report, unsigned length)
{
	unsigned end_of_foreach = 0;
	unsigned idx = 0, offset = 0, obj = 0, next = 0, bits;
	unsigned objects = 0, active_objects = 0;
	bool active_only = false, counted = false;

	memset(object, 0, sizeof(object));
	while (idx < config_length) {
		uint8_t code = config[idx++];

		switch (code) {
		case TOUCH_END:
			return;
		case TOUCH_FOREACH_ACTIVE_OBJECT:
			obj = 0;
			next = idx;
			active_only = true;
			break;
		case TOUCH_FOREACH_OBJECT:
			obj = 0;
			next = idx;
			active_only = false;
			break;
		case TOUCH_FOREACH_END:
			end_of_foreach = idx;
			if (active_only) {
				if (counted) {
					objects++;
					if (objects < active_objects) {
						idx = next;
					}
				} else if (offset < length * 8) {
					idx = next;
				}
			} else {
				obj++;
				if (obj < max_objects) {
					idx = next;
				}
			}
			break;
		case TOUCH_PAD_TO_NEXT_BYTE:
			offset = (offset + 7) / 8 * 8;
			break;
		case TOUCH_OBJECT_N_INDEX:
			CONFIG_BITS(bits);
			obj = bits_at(report, length, offset, bits);
			offset += bits;
			break;
		case TOUCH_OBJECT_N_CLASSIFICATION:
			CONFIG_BITS(bits);
			if (obj < max_objects) {
				object[obj].status = (uint8_t)bits_at(report, length, offset, bits);
			}
			offset += bits;
			break;
		case TOUCH_OBJECT_N_X_POSITION:
			CONFIG_BITS(bits);
			if (obj < max_objects) {
				object[obj].x = bits_at(report, length, offset, bits);
			}
			offset += bits;
			break;
		case TOUCH_OBJECT_N_Y_POSITION:
			CONFIG_BITS(bits);
			if (obj < max_objects) {
				object[obj].y = bits_at(report, length, offset, bits);
			}
			offset += bits;
			break;
		case TOUCH_NUM_OF_ACTIVE_OBJECTS:
			CONFIG_BITS(bits);
			active_objects = bits_at(report, length, offset, bits);
			counted = true;
			offset += bits;
			if (active_objects == 0) {
				idx = end_of_foreach;   /* no objects this frame: skip the loop's body */
			}
			break;
		default:
			/*
			 * Every other code in a report configuration carries its own bit count. The chip's own program
			 * uses codes the vendor's header does not list (0x1e, 0x0f among them), and they follow the same
			 * shape, so the bit count is taken and the field skipped: what this driver needs from the
			 * program is where each object's fields are, not what the rest mean.
			 */
			CONFIG_BITS(bits);
			offset += bits;
			break;
		}
		if (idx >= config_length) {
			return;
		}
	}
}

static uint32_t
scale(uint32_t v, uint32_t size)
{
	if (v >= size) {
		v = size - 1;
	}
	return v * 65535u / (size - 1);
}

/*
 * What the frame means, as records for /dev/hk-touch: a finger the frame names and the last one did not is down,
 * one it names again has moved, one it leaves out has lifted (the vendor's own reading of the same report).
 */
static void
post_frame(void)
{
	for (unsigned i = 0; i < max_objects; i++) {
		uint8_t now = object[i].status, before = posted[i].status;

		if (now == 0 && before == 0) {
			continue;
		}
		if (now == 0) {
			hk_tl("tl: tcm: finger %u up at %u %u of 65535\n", i, posted[i].x, posted[i].y);
			posts_up++;
			hk_touch_post(HK_TOUCH_UP, i, posted[i].x, posted[i].y);
		} else {
			uint32_t x = scale(object[i].x, panel_w), y = scale(object[i].y, panel_h);

			if (before == 0) {
				hk_tl("tl: tcm: finger %u down at %u %u of %u %u -> %u %u of 65535 (status %u)\n", i,
				    object[i].x, object[i].y, panel_w, panel_h, x, y, object[i].status);
				posts_down++;
			} else {
				posts_move++;
			}
			hk_touch_post(before == 0 ? HK_TOUCH_DOWN : HK_TOUCH_MOVE, i, x, y);
			posted[i].x = x;
			posted[i].y = y;
		}
		posted[i].status = now;
	}
	frames++;
	/*
	 * A line every 120 frames: whether the reports keep coming (the chip sends one only while it has something to
	 * say), and how many of them carried a finger down, moved it, or let it go. Without a move a drag is a tap.
	 */
	if ((frames % 120) == 0) {
		hk_tl("tl: tcm: %u reports: %u down, %u move, %u up; %u unreadable, %u bad headers, %u out of sync\n",
		    frames, posts_down, posts_move, posts_up, read_errors, bus_errors, out_of_sync);
	}
}

/* ---- the start-up ---------------------------------------------------------------------------- */

/* CMD_IDENTIFY's answer: version, mode, part number (16 bytes), build id, max write size. */
static int
identify(uint8_t *mode, char *part, unsigned part_max)
{
	uint8_t answer[64];
	unsigned length = 0, n;
	int err;

	err = tcm_command(CMD_IDENTIFY, NULL, 0, answer, &length, sizeof(answer), REPORT_IDENTIFY);
	if (err != 0) {
		return err;
	}
	hk_tl("tl: tcm: identify answered %u bytes: %02x %02x %02x ...\n", length, answer[0],
	    length > 1 ? answer[1] : 0, length > 2 ? answer[2] : 0);
	if (length < 3) {
		return EMSGSIZE;
	}
	*mode = answer[1];
	if (part != NULL) {
		n = length > 19 ? 16 : length - 2;
		if (n >= part_max) {
			n = part_max - 1;
		}
		memcpy(part, answer + 2, n);
		for (unsigned i = 0; i < n; i++) {
			if (part[i] < ' ' || part[i] > '~') {
				part[i] = '.';
			}
		}
		part[n] = 0;
	}
	return 0;
}

/* The application info: max_x at 32, max_y at 34, max_objects at 36 (syna_tcm_app_info). */
static int
application_info(void)
{
	uint8_t answer[64];
	unsigned length = 0;
	int err;

	err = tcm_command(CMD_GET_APP_INFO, NULL, 0, answer, &length, sizeof(answer), 0);
	if (err != 0) {
		return err;
	}
	if (length < 38) {
		return EMSGSIZE;
	}
	panel_w = (unsigned)answer[32] | (unsigned)answer[33] << 8;
	panel_h = (unsigned)answer[34] | (unsigned)answer[35] << 8;
	max_objects = (unsigned)answer[36] | (unsigned)answer[37] << 8;
	if (max_objects == 0 || max_objects > MAX_OBJECTS) {
		max_objects = MAX_OBJECTS;
	}
	hk_tl("tl: tcm: application %u x %u, %u objects, status %#x\n", panel_w, panel_h, max_objects,
	    (unsigned)answer[2] | (unsigned)answer[3] << 8);
	return 0;
}

static int
touch_config(void)
{
	char line[96];
	unsigned length = 0, at = 0;
	int err;

	err = tcm_command(CMD_GET_TOUCH_CONFIG, NULL, 0, config, &length, sizeof(config), 0);
	if (err != 0) {
		return err;
	}
	config_length = length;
	at += snprintf(line + at, sizeof(line) - at, "tl: tcm: report configuration %u bytes:", config_length);
	for (unsigned i = 0; i < config_length && at < (int)sizeof(line) - 4; i++) {
		if (i == 24) {
			at += snprintf(line + at, sizeof(line) - at, " ...");
			break;
		}
		at += snprintf(line + at, sizeof(line) - at, " %02x", config[i]);
	}
	hk_tl("%s\n", line);
	return 0;
}

static void
tcm_thread(void *parameter __unused, wait_result_t result __unused)
{
	uint8_t mode = 0, code, buf[TCM_MAX], line[96];
	char part[17] = "";
	unsigned length, idle = 0, errors = 0;
	int err;

	instantnoodle_gpio_input(PIN_IRQ, INSTANTNOODLE_PULL_NONE);
	instantnoodle_gpio_output(PIN_RESET, false, INSTANTNOODLE_PULL_UP, 8);  /* in reset while the supplies come up */
	if (!instantnoodle_supply_on("ldoc1", 1800) || (sleep_ms(1), !instantnoodle_supply_on("ldoa13", 3008))) {
		hk_tl("tl: tcm: no supplies (ldoc1 1800, ldoa13 3008), no touch screen\n");
		goto out;
	}
	hk_tl("tl: tcm: supplies on (ldoc1 1800, ldoa13 3008), reset 10 ms (the vendor's POWEWRUP_TO_RESET_TIME)\n");
	sleep_ms(10);
	instantnoodle_i2c_recover(&bus);
	instantnoodle_gpio_output(PIN_RESET, true, INSTANTNOODLE_PULL_UP, 8);   /* out of reset */
	sleep_ms(80);                                   /* RESET_TO_NORMAL_TIME */
	hk_tl("tl: tcm: out of reset 80 ms, report line %d\n", instantnoodle_gpio_level(PIN_IRQ));

	err = identify(&mode, part, sizeof(part));
	hk_tl("tl: tcm: identify: error %d, mode %#x, part \"%s\"\n", err, mode, part);
	if (err == 0 && mode != MODE_APPLICATION) {
		err = tcm_command(CMD_RUN_APP_FIRMWARE, NULL, 0, NULL, NULL, 0, REPORT_IDENTIFY);
		hk_tl("tl: tcm: run the application firmware: error %d\n", err);
		sleep_ms(100);
		err = identify(&mode, part, sizeof(part));
		hk_tl("tl: tcm: identify again: error %d, mode %#x, part \"%s\"\n", err, mode, part);
	}
	if (err != 0 || mode != MODE_APPLICATION) {
		hk_tl("tl: tcm: the application firmware is not running: no touch screen\n");
		goto out;
	}
	if (application_info() != 0 || touch_config() != 0) {
		hk_tl("tl: tcm: the chip did not describe itself: no touch screen\n");
		goto out;
	}
	printf("tcm: Synaptics \"%s\", panel %u x %u, %u objects, configuration %u bytes\n", part, panel_w, panel_h,
	    max_objects, config_length);
	hk_tl("tl: tcm: reading reports (report line gpio39; a read every 20 ms at the latest)\n");

	for (;;) {
		if (instantnoodle_gpio_level(PIN_IRQ)) {        /* high: nothing to collect */
			sleep_ms(1);
			if (++idle < 20) {
				continue;
			}
			idle = 0;                               /* look anyway: a missed edge must not stop the screen */
		} else {
			idle = 0;
		}
		for (unsigned drained = 0; drained < 32; drained++) {
			err = tcm_message(&code, buf, &length);
			if (err != 0) {
				if (++errors == 200) {
					hk_tl("tl: tcm: %u reads in a row failed (%u bus errors); settling\n", errors, bus_errors);
					instantnoodle_i2c_recover(&bus);
					sleep_ms(5);
					errors = 0;
				}
				break;
			}
			errors = 0;
			if (code == STATUS_IDLE || code == STATUS_BUSY) {
				break;
			}
			if (code == REPORT_TOUCH) {
				parse_touch(buf, length);
				post_frame();
			} else if (code == REPORT_IDENTIFY) {
				hk_tl("tl: tcm: the chip reset itself (identify in the report stream)\n");
			} else if (code == REPORT_STATUS) {
				if (length >= 1 && buf[0] != 0) {
					hk_tl("tl: tcm: the chip's status is %#x\n", buf[0]);
				}
			} else if (code == REPORT_PRINTF) {
				unsigned n = length > sizeof(line) - 1 ? sizeof(line) - 1 : length;

				memcpy(line, buf, n);
				line[n] = 0;
				hk_tl("tl: tcm: the chip says: %s\n", line);
			}
		}
	}
out:
	thread_terminate(current_thread());
}

void
kebab_tcm_init(void)
{
	uint32_t enabled = 1;
	thread_t thread;

	if (!kebab_board()) {
		return;
	}
	PE_parse_boot_argn("kebab_tcm", &enabled, sizeof(enabled));
	if (!enabled) {
		return;
	}
	if (kernel_thread_start_priority(tcm_thread, NULL, 92, &thread) == KERN_SUCCESS) {
		thread_deallocate(thread);
	} else {
		hk_tl("tl: tcm: the thread did not start\n");
	}
}
