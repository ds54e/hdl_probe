#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sv_vpi_user.h"
#include "svdpi.h"

#define vpi_release_handle vpi_free_object

enum {
  HDL_PROBE_OK = 0,
  HDL_PROBE_NOT_FOUND = 1,
  HDL_PROBE_UNSUPPORTED_KIND = 2,
  HDL_PROBE_INTERNAL_ERROR = 5
};

enum {
  HDL_PROBE_KIND_UNKNOWN = 0,
  HDL_PROBE_KIND_LOGIC4 = 1,
  HDL_PROBE_KIND_REAL = 2
};

typedef struct hdl_probe_s {
  struct hdl_probe_s *self_check;
  vpiHandle obj;
  vpiHandle cb;
  int sv_key;
  int kind;
  int size;
  unsigned int top_mask;
} hdl_probe_t;

extern void hdl_probe_value_change_notify(int sv_key);

static svScope g_pkg_scope = NULL;

static void report_error(const char *fmt, const char *arg) {
  if (arg != NULL) {
    vpi_printf((PLI_BYTE8 *)"HDL_PROBE ERROR: ");
    vpi_printf((PLI_BYTE8 *)fmt, arg);
    vpi_printf((PLI_BYTE8 *)"\n");
  } else {
    vpi_printf((PLI_BYTE8 *)"HDL_PROBE ERROR: %s\n", fmt);
  }
}

static hdl_probe_t *checked_probe(void *hnd) {
  hdl_probe_t *probe = (hdl_probe_t *)hnd;
  if (probe != NULL && probe->self_check == probe) {
    return probe;
  }

  report_error("bad probe handle", NULL);
  return NULL;
}

static int get_typespec_kind(vpiHandle obj) {
  vpiHandle typespec = vpi_handle(vpiTypespec, obj);
  if (typespec == NULL) {
    return HDL_PROBE_KIND_UNKNOWN;
  }

  switch (vpi_get(vpiType, typespec)) {
    case vpiEnumTypespec:
    case vpiBitTypespec:
    case vpiLogicTypespec:
    case vpiIntTypespec:
    case vpiIntegerTypespec:
    case vpiLongIntTypespec:
    case vpiShortIntTypespec:
    case vpiByteTypespec:
      return HDL_PROBE_KIND_LOGIC4;
    case vpiRealTypespec:
      return HDL_PROBE_KIND_REAL;
    default:
      return HDL_PROBE_KIND_UNKNOWN;
  }
}

static int object_kind(vpiHandle obj) {
  int obj_type = vpi_get(vpiType, obj);

  switch (obj_type) {
    case vpiRealVar:
      return HDL_PROBE_KIND_REAL;

    case vpiNet:
    case vpiNetBit:
    case vpiReg:
    case vpiRegBit:
    case vpiPartSelect:
    case vpiBitSelect:
    case vpiBitVar:
    case vpiEnumVar:
    case vpiEnumNet:
    case vpiIntVar:
    case vpiLongIntVar:
    case vpiShortIntVar:
    case vpiIntegerVar:
    case vpiByteVar:
      return HDL_PROBE_KIND_LOGIC4;

    default:
      return get_typespec_kind(obj);
  }
}

static PLI_INT32 hdl_probe_dpi_value_change_callback(p_cb_data cb_data) {
  hdl_probe_t *probe = checked_probe(cb_data->user_data);
  svScope previous_scope;

  if (probe == NULL) {
    return 0;
  }

  previous_scope = svGetScope();
  if (g_pkg_scope != NULL) {
    svSetScope(g_pkg_scope);
  }
  hdl_probe_value_change_notify(probe->sv_key);
  if (g_pkg_scope != NULL && previous_scope != NULL) {
    svSetScope(previous_scope);
  }

  return 1;
}

void *hdl_probe_dpi_create(const char *path, int sv_key, int *status) {
  vpiHandle obj;
  hdl_probe_t *probe;
  int kind;
  int size;

  obj = vpi_handle_by_name((PLI_BYTE8 *)path, NULL);
  if (obj == NULL) {
    if (status != NULL) {
      *status = HDL_PROBE_NOT_FOUND;
    }
    return NULL;
  }

  kind = object_kind(obj);
  if (kind == HDL_PROBE_KIND_UNKNOWN) {
    if (status != NULL) {
      *status = HDL_PROBE_UNSUPPORTED_KIND;
    }
    vpi_release_handle(obj);
    return NULL;
  }

  probe = (hdl_probe_t *)calloc(1, sizeof(*probe));
  if (probe == NULL) {
    if (status != NULL) {
      *status = HDL_PROBE_INTERNAL_ERROR;
    }
    vpi_release_handle(obj);
    return NULL;
  }

  size = (kind == HDL_PROBE_KIND_REAL) ? 64 : vpi_get(vpiSize, obj);

  probe->self_check = probe;
  probe->obj = obj;
  probe->sv_key = sv_key;
  probe->kind = kind;
  probe->size = size;
  probe->cb = NULL;
  if (kind == HDL_PROBE_KIND_LOGIC4 && size > 0) {
    probe->top_mask = (size % 32 == 0) ? 0xffffffffu : ((1u << (size % 32)) - 1u);
  } else {
    probe->top_mask = 0xffffffffu;
  }

  if (status != NULL) {
    *status = HDL_PROBE_OK;
  }
  return probe;
}

int hdl_probe_dpi_capture_callback_scope(void) {
  g_pkg_scope = svGetScope();
  return (g_pkg_scope != NULL);
}

int hdl_probe_dpi_clear_callback_scope(void) {
  g_pkg_scope = NULL;
  return 1;
}

int hdl_probe_dpi_destroy(void *hnd) {
  hdl_probe_t *probe = checked_probe(hnd);

  if (probe == NULL) {
    return 0;
  }

  if (probe->cb != NULL) {
    vpi_remove_cb(probe->cb);
    probe->cb = NULL;
  }
  if (probe->obj != NULL) {
    vpi_release_handle(probe->obj);
    probe->obj = NULL;
  }

  probe->self_check = NULL;
  free(probe);
  return 1;
}

int hdl_probe_dpi_set_enable_callback(void *hnd, int enable) {
  s_cb_data cb_data;
  s_vpi_time time_s;
  s_vpi_value value_s;
  hdl_probe_t *probe = checked_probe(hnd);

  if (probe == NULL) {
    return 0;
  }

  if (enable) {
    if (probe->cb != NULL) {
      return 1;
    }

    memset(&cb_data, 0, sizeof(cb_data));
    memset(&time_s, 0, sizeof(time_s));
    memset(&value_s, 0, sizeof(value_s));

    cb_data.reason = cbValueChange;
    cb_data.cb_rtn = &hdl_probe_dpi_value_change_callback;
    cb_data.obj = probe->obj;
    cb_data.time = &time_s;
    time_s.type = vpiSuppressTime;
    cb_data.value = &value_s;
    value_s.format = vpiSuppressVal;
    cb_data.user_data = (PLI_BYTE8 *)probe;
    probe->cb = vpi_register_cb(&cb_data);
    return (probe->cb != NULL);
  }

  if (probe->cb != NULL) {
    vpi_remove_cb(probe->cb);
    probe->cb = NULL;
  }
  return 1;
}

int hdl_probe_dpi_get_kind(void *hnd) {
  hdl_probe_t *probe = checked_probe(hnd);
  return (probe == NULL) ? HDL_PROBE_KIND_UNKNOWN : probe->kind;
}

int hdl_probe_dpi_get_size(void *hnd) {
  hdl_probe_t *probe = checked_probe(hnd);
  return (probe == NULL) ? 0 : probe->size;
}

int hdl_probe_dpi_read_logic_words(void *hnd, const svOpenArrayHandle avals,
                                   const svOpenArrayHandle bvals) {
  hdl_probe_t *probe = checked_probe(hnd);
  s_vpi_value value_s;
  p_vpi_vecval vec;
  svLogicVecVal word;
  unsigned int *dst_aval;
  unsigned int *dst_bval;
  int array_size;
  int low;
  int increment;
  int num_chunks;

  if (probe == NULL || probe->kind != HDL_PROBE_KIND_LOGIC4 || avals == NULL ||
      bvals == NULL) {
    return 0;
  }

  if (svDimensions(avals) != 1 || svDimensions(bvals) != 1) {
    return 0;
  }

  num_chunks = (probe->size + 31) / 32;
  array_size = svSize(avals, 1);
  if (array_size < num_chunks) {
    return 0;
  }
  if (svSize(bvals, 1) < num_chunks) {
    return 0;
  }

  low = svLow(avals, 1);
  increment = svIncrement(avals, 1);
  if (svLow(bvals, 1) != low || svIncrement(bvals, 1) != increment) {
    return 0;
  }

  value_s.format = vpiVectorVal;
  vpi_get_value(probe->obj, &value_s);
  vec = value_s.value.vector;

  for (int chunk = 0; chunk < num_chunks; chunk++) {
    word = vec[chunk];
    if (chunk == (num_chunks - 1) && (probe->size % 32) != 0) {
      word.aval &= probe->top_mask;
      word.bval &= probe->top_mask;
    }
    dst_aval = (unsigned int *)svGetArrElemPtr1(avals, low + (chunk * increment));
    dst_bval = (unsigned int *)svGetArrElemPtr1(bvals, low + (chunk * increment));
    if (dst_aval == NULL || dst_bval == NULL) {
      return 0;
    }
    *dst_aval = word.aval;
    *dst_bval = word.bval;
  }

  return 1;
}

int hdl_probe_dpi_read_real(void *hnd, double *value) {
  hdl_probe_t *probe = checked_probe(hnd);
  s_vpi_value value_s;

  if (probe == NULL || probe->kind != HDL_PROBE_KIND_REAL) {
    return 0;
  }

  value_s.format = vpiRealVal;
  vpi_get_value(probe->obj, &value_s);
  *value = value_s.value.real;
  return 1;
}
