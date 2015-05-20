/*
 * Copyright (c) 2005 Palmsource, Inc.
 * 
 * This software is licensed as described in the file LICENSE, which
 * you should have received as part of this distribution. The terms
 * are also available at http://www.openbinder.org/license.html.
 * 
 * This software consists of voluntary contributions made by many
 * individuals. For the exact contribution history, see the revision
 * history and logs, available at http://www.openbinder.org
 */

#include <support/Debug.h>
#include <sys/types.h>
#include <support/SupportDefs.h>
#include <support/ByteOrder.h>
#include <support/Value.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

int is_type_swapped(type_code type)
{
	switch (type) {
		case B_BOOL_TYPE:
		case B_CHAR_TYPE:
		case B_DOUBLE_TYPE:
		case B_FLOAT_TYPE:
		case B_INT64_TYPE:
		case B_INT32_TYPE:
		case B_INT16_TYPE:
		case B_INT8_TYPE:
		case B_MIME_TYPE:
		case B_OFF_T_TYPE:
		case B_POINT_TYPE:
		case B_RECT_TYPE:
		case B_COLOR_32_TYPE:
		case B_SIZE_T_TYPE:
		case B_SSIZE_T_TYPE:
		case B_STRING_TYPE:
		case B_TIME_TYPE:
		case B_UINT64_TYPE:
		case B_UINT32_TYPE:
		case B_UINT16_TYPE:
		case B_UINT8_TYPE:
			return true;
	}
	return false;
}

status_t swap_data(type_code type, void *d, size_t length,
	swap_action action)
{
	if ((action == B_SWAP_HOST_TO_LENDIAN) && (B_HOST_IS_LENDIAN))
		return B_OK;
	if ((action == B_SWAP_LENDIAN_TO_HOST) && (B_HOST_IS_LENDIAN))
		return B_OK;

	if ((action == B_SWAP_HOST_TO_BENDIAN) && (B_HOST_IS_BENDIAN))
		return B_OK;
	if ((action == B_SWAP_BENDIAN_TO_HOST) && (B_HOST_IS_BENDIAN))
		return B_OK;

	char	*data = (char *) d;
	char	*end = data + length;

	status_t	err = B_OK;

	switch (type) {
		case B_INT16_TYPE:
		case B_UINT16_TYPE: {
			while (data < end) {
				*((int16_t *) data) = _swap_int16(*((int16_t *) data));
				data += sizeof(int16_t);
			}
			break;
		}
		case B_SIZE_T_TYPE:			// ??? could be diff size on diff platform
		case B_SSIZE_T_TYPE: {		// ??? could be diff size on diff platform
			ASSERT(sizeof(size_t) == sizeof(int32_t));
			ASSERT(sizeof(ssize_t) == sizeof(int32_t));
			while (data < end) {
				*((int32_t *) data) = _swap_int32(*((int32_t *) data));
				data += sizeof(int32_t);
			}
			break;
		}
		case B_TIME_TYPE: {
			ASSERT(sizeof(time_t) == sizeof(int32_t));
			while (data < end) {
				*((int32_t *) data) = _swap_int32(*((int32_t *) data));
				data += sizeof(int32_t);
			}
			break;
		}
		case B_INT32_TYPE:
		case B_UINT32_TYPE: {
			while (data < end) {
				*((int32_t *) data) = _swap_int32(*((int32_t *) data));
				data += sizeof(int32_t);
			}
			break;
		}
		case B_OFF_T_TYPE: {		// ??? could be diff size on diff platform
			ASSERT(sizeof(off_t) == sizeof(int64_t));
			while (data < end) {
				*((int64_t *) data) = _swap_int64(*((int64_t *) data));
				data += sizeof(int64_t);
			}
			break;
		}
		case B_INT64_TYPE:
		case B_UINT64_TYPE: {
			while (data < end) {
				*((int64_t *) data) = _swap_int64(*((int64_t *) data));
				data += sizeof(int64_t);
			}
			break;
		}
		case B_RECT_TYPE: {
			ASSERT(sizeof(coord) == sizeof(float));
			coord *r;
			while (data < end) {
				r = (coord *)data;
				r[0] = _swap_float(r[0]);
				r[1] = _swap_float(r[1]);
				r[2] = _swap_float(r[2]);
				r[3] = _swap_float(r[3]);
				data += sizeof(coord)*4;
			}
			break;
		}
		case B_POINT_TYPE: {
			ASSERT(sizeof(coord) == sizeof(float));
			coord *pt;
			while (data < end) {
				pt = (coord *)data;
				pt[0] = _swap_float(pt[0]);
				pt[1] = _swap_float(pt[1]);
				data += sizeof(coord)*2;
			}
			break;
		}
		case B_FLOAT_TYPE: {
			while (data < end) {
				*((float *) data) = _swap_float(*((float *) data));
				data += sizeof(float);
			}
			break;
		}
		case B_DOUBLE_TYPE: {
			while (data < end) {
				*((double *) data) = _swap_double(*((double *) data));
				data += sizeof(double);
			}
			break;
		}
		default: {
			err = B_BAD_VALUE;	// can't swap this type
			break;
		}
	}
	return err;
}
