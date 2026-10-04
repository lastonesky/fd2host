/* winshot.h - capture what the window actually shows.
 *
 * --screenshot dumps the shared layer's buffer (before any backend sees it),
 * so it cannot prove that a backend presents correctly. --wshot captures the
 * window itself; comparing the two for the *same run* is a timing-independent
 * proof that backend -> screen matches the shared layer (PROGRESS.md §13.6
 * step 2 acceptance). */
#ifndef FD2_WINSHOT_H
#define FD2_WINSHOT_H

/* hwnd: platform window handle. Returns 0 on success. */
int winshot_capture(void *hwnd, const char *path);

#endif /* FD2_WINSHOT_H */
