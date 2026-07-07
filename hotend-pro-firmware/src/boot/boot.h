#pragma once
/* ============================================================
 * Boot screen — animated logo with fade-in / fade-out
 * ============================================================ */

#ifdef __cplusplus
extern "C" {
#endif

/* Blocking call — shows boot screen then returns when done. */
void boot_screen_run(void);

#ifdef __cplusplus
}
#endif
