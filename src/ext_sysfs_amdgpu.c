/**
 * Author......: See docs/credits.txt
 * License.....: MIT
 */

#include "common.h"
#include "types.h"
#include "memory.h"
#include "shared.h"
#include "filehandling.h"
#include "path.h"
#include "event.h"
#include "folder.h"
#include "ext_sysfs_amdgpu.h"

#include <setjmp.h>
#include <signal.h>
#include <unistd.h>

bool sysfs_amdgpu_init (void *hashcat_ctx)
{
  hwmon_ctx_t *hwmon_ctx = ((hashcat_ctx_t *) hashcat_ctx)->hwmon_ctx;

  SYSFS_AMDGPU_PTR *sysfs_amdgpu = (SYSFS_AMDGPU_PTR *) hwmon_ctx->hm_sysfs_amdgpu;

  memset (sysfs_amdgpu, 0, sizeof (SYSFS_AMDGPU_PTR));

  char *path;

  hc_asprintf (&path, "%s", SYS_BUS_PCI_DEVICES);

  const bool r = hc_path_read (path);

  hcfree (path);

  return r;
}

void sysfs_amdgpu_close (void *hashcat_ctx)
{
  hwmon_ctx_t *hwmon_ctx = ((hashcat_ctx_t *) hashcat_ctx)->hwmon_ctx;

  SYSFS_AMDGPU_PTR *sysfs_amdgpu = (SYSFS_AMDGPU_PTR *) hwmon_ctx->hm_sysfs_amdgpu;

  if (sysfs_amdgpu)
  {
    hcfree (sysfs_amdgpu);
  }
}

// SMU backed sysfs attributes can block indefinitely on some boards, for example while a
// governor daemon races the power state machine. A read that never returns would hang the
// whole session, so every read through sysfs_fgets_timeout () is bounded by a watchdog alarm.

// the watchdog needs the POSIX alarm machinery. On Windows there is no sysfs to hang on, so
// the read falls back to a plain fgets.

#define SYSFS_AMDGPU_READ_TIMEOUT 2

#ifndef _WIN32

static sigjmp_buf sysfs_amdgpu_jmp;

static volatile sig_atomic_t sysfs_amdgpu_jump = 0;

static void sysfs_amdgpu_watchdog_handler (int signum)
{
  if (sysfs_amdgpu_jump == 1)
  {
    siglongjmp (sysfs_amdgpu_jmp, 1);
  }
}

static char *sysfs_fgets_timeout (char *buf, const int len, HCFILE *fp)
{
  struct sigaction sa;
  struct sigaction old;

  memset (&sa, 0, sizeof (sa));

  sa.sa_handler = sysfs_amdgpu_watchdog_handler;

  char *r = NULL;

  sigaction (SIGALRM, &sa, &old);

  if (sigsetjmp (sysfs_amdgpu_jmp, 1) == 0)
  {
    sysfs_amdgpu_jump = 1;

    alarm (SYSFS_AMDGPU_READ_TIMEOUT);

    r = hc_fgets (buf, len, fp);

    alarm (0);
  }

  sysfs_amdgpu_jump = 0;

  sigaction (SIGALRM, &old, NULL);

  return r;
}

#else

static char *sysfs_fgets_timeout (char *buf, const int len, HCFILE *fp)
{
  return hc_fgets (buf, len, fp);
}

#endif

char *hm_SYSFS_AMDGPU_get_syspath_device (void *hashcat_ctx, const int backend_device_idx)
{
  backend_ctx_t *backend_ctx = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx;

  hc_device_param_t *device_param = &backend_ctx->devices_param[backend_device_idx];

  char *syspath;

  hc_asprintf (&syspath, "%s/0000:%02x:%02x.%01x", SYS_BUS_PCI_DEVICES, device_param->pcie_bus, device_param->pcie_device, device_param->pcie_function);

  return syspath;
}

char *hm_SYSFS_AMDGPU_get_syspath_hwmon (void *hashcat_ctx, const int backend_device_idx)
{
  char *syspath = hm_SYSFS_AMDGPU_get_syspath_device (hashcat_ctx, backend_device_idx);

  if (syspath == NULL)
  {
    event_log_error (hashcat_ctx, "hm_SYSFS_AMDGPU_get_syspath_device() failed.");

    return NULL;
  }

  char *hwmon = (char *) hcmalloc (HCBUFSIZ_TINY);

  snprintf (hwmon, HCBUFSIZ_TINY, "%s/hwmon", syspath);

  char *hwmonN = first_file_in_directory (hwmon);

  if (hwmonN == NULL)
  {
    event_log_error (hashcat_ctx, "First_file_in_directory() failed.");

    hcfree (syspath);

    hcfree (hwmon);
    hcfree (hwmonN);

    return NULL;
  }

  snprintf (hwmon, HCBUFSIZ_TINY, "%s/hwmon/%s", syspath, hwmonN);

  hcfree (syspath);

  hcfree (hwmonN);

  return hwmon;
}

int hm_SYSFS_AMDGPU_get_fan_speed_current (void *hashcat_ctx, const int backend_device_idx, int *val)
{
  backend_ctx_t *backend_ctx = ((hashcat_ctx_t *) hashcat_ctx)->backend_ctx;

  hc_device_param_t *device_param = &backend_ctx->devices_param[backend_device_idx];

  if (device_param->device_host_unified_memory == 1)
  {
    *val = 0;

    return 0;
  }

  char *syspath = hm_SYSFS_AMDGPU_get_syspath_hwmon (hashcat_ctx, backend_device_idx);

  if (syspath == NULL) return -1;

  char *path_cur;
  char *path_max;

  hc_asprintf (&path_cur, "%s/pwm1",     syspath);
  hc_asprintf (&path_max, "%s/pwm1_max", syspath);

  hcfree (syspath);

  // many boards have no pwm capable gpu fan, the pwm then sits on a separate
  // super i/o chip outside of the gpu hwmon, so fail quietly when it is absent

  if (hc_path_read (path_cur) == false)
  {
    hcfree (path_cur);
    hcfree (path_max);

    return -1;
  }

  HCFILE fp_cur;

  if (hc_fopen (&fp_cur, path_cur, "r") == false)
  {
    event_log_error (hashcat_ctx, "%s: %s", path_cur, strerror (errno));

    hcfree (path_cur);
    hcfree (path_max);

    return -1;
  }

  int pwm1_cur = 0;

  if (hc_fscanf (&fp_cur, "%d", &pwm1_cur) != 1)
  {
    hc_fclose (&fp_cur);

    event_log_error (hashcat_ctx, "%s: unexpected data.", path_cur);

    hcfree (path_cur);
    hcfree (path_max);

    return -1;
  }

  hc_fclose (&fp_cur);

  HCFILE fp_max;

  if (hc_fopen (&fp_max, path_max, "r") == false)
  {
    event_log_error (hashcat_ctx, "%s: %s", path_max, strerror (errno));

    hcfree (path_cur);
    hcfree (path_max);

    return -1;
  }

  int pwm1_max = 0;

  if (hc_fscanf (&fp_max, "%d", &pwm1_max) != 1)
  {
    hc_fclose (&fp_max);

    event_log_error (hashcat_ctx, "%s: unexpected data.", path_max);

    hcfree (path_cur);
    hcfree (path_max);

    return -1;
  }

  hc_fclose (&fp_max);

  if (pwm1_max == 0)
  {
    event_log_error (hashcat_ctx, "%s: pwm1_max cannot be 0.", path_max);

    hcfree (path_cur);
    hcfree (path_max);

    return -1;
  }

  const float p1 = (float) pwm1_max / 100.0F;

  const float pwm1_percent = (float) pwm1_cur / p1;

  *val = (int) pwm1_percent;

  hcfree (path_cur);
  hcfree (path_max);

  return 0;
}

int hm_SYSFS_AMDGPU_get_temperature_current (void *hashcat_ctx, const int backend_device_idx, int *val)
{
  char *syspath = hm_SYSFS_AMDGPU_get_syspath_hwmon (hashcat_ctx, backend_device_idx);

  if (syspath == NULL) return -1;

  char *path;

  hc_asprintf (&path, "%s/temp1_input", syspath);

  hcfree (syspath);

  HCFILE fp;

  if (hc_fopen (&fp, path, "r") == false)
  {
    event_log_error (hashcat_ctx, "%s: %s", path, strerror (errno));

    hcfree (path);

    return -1;
  }

  int temperature = 0;

  char buf[HCBUFSIZ_TINY] = { 0 };

  if (sysfs_fgets_timeout (buf, sizeof (buf), &fp) == NULL)
  {
    hc_fclose (&fp);

    hcfree (path);

    return -1;
  }

  if (sscanf (buf, "%d", &temperature) != 1)
  {
    hc_fclose (&fp);

    hcfree (path);

    return -1;
  }

  hc_fclose (&fp);

  *val = temperature / 1000;

  hcfree (path);

  return 0;
}

int hm_SYSFS_AMDGPU_get_pp_dpm_sclk (void *hashcat_ctx, const int backend_device_idx, int *val)
{
  // some boards (BC-250 with UMA among them) run a DPM table whose entries carry
  // nonsense clocks, and the entry the firmware marks current can read 5..20 MHz
  // while the engine actually runs at full speed. The hwmon exposes the measured
  // clock of the engine itself, so that reading is preferred when it exists.

  char *hmpath = hm_SYSFS_AMDGPU_get_syspath_hwmon (hashcat_ctx, backend_device_idx);

  if (hmpath != NULL)
  {
    char *path;

    hc_asprintf (&path, "%s/freq1_input", hmpath);

    hcfree (hmpath);

    if (hc_path_read (path) == true)
    {
      HCFILE fp;

      if (hc_fopen (&fp, path, "r") == true)
      {
        char buf[HCBUFSIZ_TINY] = { 0 };

        char *ptr = sysfs_fgets_timeout (buf, sizeof (buf), &fp);

        hc_fclose (&fp);

        if (ptr != NULL)
        {
          long hz = strtol (ptr, NULL, 10);

          if (hz > 0)
          {
            *val = (int) (hz / 1000000L);

            hcfree (path);

            return 0;
          }
        }
      }
    }

    hcfree (path);
  }

  char *syspath = hm_SYSFS_AMDGPU_get_syspath_device (hashcat_ctx, backend_device_idx);

  if (syspath == NULL) return -1;

  char *path;

  hc_asprintf (&path, "%s/pp_dpm_sclk", syspath);

  hcfree (syspath);

  HCFILE fp;

  if (hc_fopen (&fp, path, "r") == false)
  {
    event_log_error (hashcat_ctx, "%s: %s", path, strerror (errno));

    hcfree (path);

    return -1;
  }

  int clockfreq = 0;

  while (!hc_feof (&fp))
  {
    char buf[HCBUFSIZ_TINY] = { 0 };

    char *ptr = sysfs_fgets_timeout (buf, sizeof (buf), &fp);

    if (ptr == NULL) break;

    size_t len = strlen (ptr);

    if (len < 2) continue;

    if (ptr[len - 2] != '*') continue;

    int profile = 0;

    int rc = sscanf (ptr, "%d: %dMHz", &profile, &clockfreq);

    if (rc == 2) break;
  }

  hc_fclose (&fp);

  *val = clockfreq;

  hcfree (path);

  return 0;
}

int hm_SYSFS_AMDGPU_get_pp_dpm_mclk (void *hashcat_ctx, const int backend_device_idx, int *val)
{
  char *syspath = hm_SYSFS_AMDGPU_get_syspath_device (hashcat_ctx, backend_device_idx);

  if (syspath == NULL) return -1;

  char *path;

  hc_asprintf (&path, "%s/pp_dpm_mclk", syspath);

  hcfree (syspath);

  HCFILE fp;

  if (hc_fopen (&fp, path, "r") == false)
  {
    event_log_error (hashcat_ctx, "%s: %s", path, strerror (errno));

    hcfree (path);

    return -1;
  }

  int clockfreq = 0;

  while (!hc_feof (&fp))
  {
    char buf[HCBUFSIZ_TINY];

    char *ptr = sysfs_fgets_timeout (buf, sizeof (buf), &fp);

    if (ptr == NULL) break;

    size_t len = strlen (ptr);

    if (len < 2) continue;

    if (ptr[len - 2] != '*') continue;

    int profile = 0;

    int rc = sscanf (ptr, "%d: %dMHz", &profile, &clockfreq);

    if (rc == 2) break;
  }

  hc_fclose (&fp);

  *val = clockfreq;

  hcfree (path);

  return 0;
}

int hm_SYSFS_AMDGPU_get_pp_dpm_pcie (void *hashcat_ctx, const int backend_device_idx, int *val)
{
  char *syspath = hm_SYSFS_AMDGPU_get_syspath_device (hashcat_ctx, backend_device_idx);

  if (syspath == NULL) return -1;

  char *path;

  hc_asprintf (&path, "%s/current_link_width", syspath);

  hcfree (syspath);

  HCFILE fp;

  if (hc_fopen (&fp, path, "r") == false)
  {
    event_log_error (hashcat_ctx, "%s: %s", path, strerror (errno));

    hcfree (path);

    return -1;
  }

  int lanes = 0;

  while (!hc_feof (&fp))
  {
    char buf[HCBUFSIZ_TINY];

    char *ptr = sysfs_fgets_timeout (buf, sizeof (buf), &fp);

    if (ptr == NULL) break;

    size_t len = strlen (ptr);

    if (len < 2) continue;

    int rc = sscanf (ptr, "%d", &lanes);

    if (rc == 1) break;
  }

  hc_fclose (&fp);

  *val = lanes;

  hcfree (path);

  return 0;
}

// the plain gpu_busy_percent reader, kept separate because the metrics reader above
// falls back to it

static int hm_SYSFS_AMDGPU_get_gpu_busy_percent_fallback (void *hashcat_ctx, const int backend_device_idx, int *val)
{
  char *syspath = hm_SYSFS_AMDGPU_get_syspath_device (hashcat_ctx, backend_device_idx);

  if (syspath == NULL) return -1;

  char *path;

  hc_asprintf (&path, "%s/gpu_busy_percent", syspath);

  hcfree (syspath);

  HCFILE fp;

  if (hc_fopen (&fp, path, "r") == false)
  {
    event_log_error (hashcat_ctx, "%s: %s", path, strerror (errno));

    hcfree (path);

    return -1;
  }

  int util = 0;

  while (!hc_feof (&fp))
  {
    char buf[HCBUFSIZ_TINY];

    char *ptr = sysfs_fgets_timeout (buf, sizeof (buf), &fp);

    if (ptr == NULL) break;

    size_t len = strlen (ptr);

    if (len < 1) continue;

    int rc = sscanf (ptr, "%d", &util);

    if (rc == 1) break;
  }

  hc_fclose (&fp);

  *val = util;

  hcfree (path);

  return 0;
}

int hm_SYSFS_AMDGPU_get_gpu_busy_percent (void *hashcat_ctx, const int backend_device_idx, int *val)
{
  // gpu_busy_percent is answered with "Not supported" on some kernels (BC-250 among
  // them), so the binary gpu_metrics blob is read first. Its gfx activity field
  // carries the engine busy percentage in 0.1% steps. The blob layout is not
  // versioned in a way a reader could rely on across families, so the offset used
  // here is the one the tested board answers with and every field is sanity checked
  // before it is trusted. The sysfs attribute stays as the fallback.

  char *syspath = hm_SYSFS_AMDGPU_get_syspath_device (hashcat_ctx, backend_device_idx);

  if (syspath == NULL) return -1;

  char *path;

  hc_asprintf (&path, "%s/gpu_metrics", syspath);

  hcfree (syspath);

  if (hc_path_read (path) == true)
  {
    HCFILE fp;

    if (hc_fopen (&fp, path, "r") == true)
    {
      unsigned char blob[1024];

      const size_t n = hc_fread (blob, 1, sizeof (blob), &fp);

      hc_fclose (&fp);

      // the tested firmware answers with a 128 byte blob in which the u16 at
      // offset 0x40 is the gfx activity in 0.1% steps

      if (n >= 0x42)
      {
        const u16 activity = (u16) (blob[0x40] | (blob[0x41] << 8));

        if ((activity != 0xFFFF) && (activity <= 1000))
        {
          *val = (int) ((activity + 5) / 10);

          hcfree (path);

          return 0;
        }
      }
    }
  }

  hcfree (path);

  return hm_SYSFS_AMDGPU_get_gpu_busy_percent_fallback (hashcat_ctx, backend_device_idx, val);
}

int hm_SYSFS_AMDGPU_get_mem_info_vram_used (void *hashcat_ctx, const int backend_device_idx, u64 *val)
{
  char *syspath = hm_SYSFS_AMDGPU_get_syspath_device (hashcat_ctx, backend_device_idx);

  if (syspath == NULL) return -1;

  char *path;

  hc_asprintf (&path, "%s/mem_info_vram_used", syspath);

  hcfree (syspath);

  HCFILE fp;

  if (hc_fopen (&fp, path, "r") == false)
  {
    event_log_error (hashcat_ctx, "%s: %s", path, strerror (errno));

    hcfree (path);

    return -1;
  }

  u64 mem_info_vram_used = 0;

  while (!hc_feof (&fp))
  {
    char buf[HCBUFSIZ_TINY];

    char *ptr = sysfs_fgets_timeout (buf, sizeof (buf), &fp);

    if (ptr == NULL) break;

    size_t len = strlen (ptr);

    if (len < 1) continue;

    int rc = sscanf (ptr, "%" PRIu64, &mem_info_vram_used);

    if (rc == 1) break;
  }

  hc_fclose (&fp);

  *val = mem_info_vram_used;

  hcfree (path);

  return 0;
}

