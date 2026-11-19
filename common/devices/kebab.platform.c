/*
 * kebab: paste into xnuloader/loader.c next to platform_redwood and add &platform_kebab to platforms[].
 * Every value is explained in docs/PORTING.md ("The loader").
 *
 * GIC, cores and the peripheral window are the SM8250/kona values already used by platform_instantnoodle
 * and platform_instantnoodlep; the phone's own DTB (vendor_boot or /sys/firmware/fdt with root) should
 * still be checked with tools/board-from-dt.py. fb_base is the ABL's continuous splash, as on the
 * other OnePlus kona phones. fb_height 2400 is shared with the OnePlus 8, so the loader also accepts
 * an explicit "platform=kebab" payload entry (common/build.sh and xl send it); that entry is what
 * makes the board= argument unambiguous on this phone.
 */
static const struct platform platform_kebab = {
	.name = "kebab", .io_base = 0x00400000, .io_size = 0x3FC00000,
	.gic_dist = 0x17A00000, .gic_redist = 0x17A60000, .uart = 0, .log_other_cpu = 1,
	.cores = 8, .core_shift = 8,
	.small_cores = 0x0f,                            /* four Cortex-A55s */
	.fb_base = 0x9C000000, .fb_height = 2400,
	/* the port's own partition would be carved off userdata, as on the 8 Pro (not used in RAM runs) */
	.store = "darwin", .store_type = BLOCK_DARWIN_TYPE, .store_uniq = BLOCK_DARWIN_UNIQ,
	.store_first = BLOCK_DARWIN_FIRST, .store_last = BLOCK_DARWIN_LAST,
	.board_args = "board_thermal_gpuss_0=0xf board_thermal_gpuss_1=0x108",
	/* first-boot line; from the 8T's own DTB/ABL (panel 1080x2400 at 120 Hz, S6SY761 touch) */
	.default_args = "cpus=8 bootcpu=7 cpufreq=1 fb_hz=120 fb_push=2 fbwidth=1080 fbheight=2400",
};
