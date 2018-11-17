//-----------------------------------------------------------------------------
// Copyright (C) 2018 Merlok
//
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// Tools for work with CBOR format http://cbor.io/spec.html
// via Intel tinycbor (https://github.com/intel/tinycbor) library
//-----------------------------------------------------------------------------
//

#include "cbortools.h"
#include <stdlib.h>
#include "cbor.h"
#include "util.h"
#include "fidocore.h"

static void indent(int nestingLevel) {
	while (nestingLevel--)
		printf("  ");
}

static CborError dumpelm(CborValue *it, bool *got_next, int nestingLevel) {
	CborError err;
	*got_next = false;

	CborType type = cbor_value_get_type(it);
	indent(nestingLevel);
	switch (type) {
	case CborMapType: 
	case CborArrayType: {
		printf(type == CborArrayType ? "Array[" : "Map[");
		break;
	}

	case CborIntegerType: {
		int64_t val;
		cbor_value_get_int64(it, &val);     // can't fail
		printf("%lld", (long long)val);
		break;
	}

	case CborByteStringType: {
		uint8_t *buf;
		size_t n;
		err = cbor_value_dup_byte_string(it, &buf, &n, it);
		*got_next = true;
		if (err)
			return err;     // parse error
		printf("%s", sprint_hex(buf, n));
		free(buf);
		break;
	}

	case CborTextStringType: {
		char *buf;
		size_t n;
		err = cbor_value_dup_text_string(it, &buf, &n, it);
		*got_next = true;
		if (err)
			return err;     // parse error
		printf("%s", buf);
		free(buf);
		break;
	}

	case CborTagType: {
		CborTag tag;
		cbor_value_get_tag(it, &tag);     
		printf("Tag(%lld)", (long long)tag);
		break;
	}

	case CborSimpleType: {
		uint8_t type;
		cbor_value_get_simple_type(it, &type); 
		printf("simple(%u)", type);
		break;
	}

	case CborNullType:
		printf("null");
		break;

	case CborUndefinedType:
		printf("undefined");
		break;

	case CborBooleanType: {
		bool val;
		cbor_value_get_boolean(it, &val);       // can't fail
		printf("%s", val ? "true" : "false");
		break;
	}

	case CborDoubleType: {
		double val;
		if (false) {
			float f;
	case CborFloatType:
			cbor_value_get_float(it, &f);
			val = f;
		} else {
			cbor_value_get_double(it, &val);
		}
		printf("%g", val);
		break;
	}
	case CborHalfFloatType: {
		uint16_t val;
		cbor_value_get_half_float(it, &val);
		printf("__f16(%04x)", val);
		break;
	}

	case CborInvalidType:
		printf("CborInvalidType!!!");
		break;
	}
	
	return CborNoError;
}

static CborError dumprecursive(uint8_t cmdCode, CborValue *it, bool isMapType, int nestingLevel) {
	int elmCount = 0;
	while (!cbor_value_at_end(it)) {
		CborError err;
		CborType type = cbor_value_get_type(it);
//printf("^%x^", type);
		bool got_next;

		switch (type) {
		case CborMapType: 
		case CborArrayType: {
			// recursive type
			CborValue recursed;
			assert(cbor_value_is_container(it));
			if (!(isMapType && (elmCount % 2)))
				indent(nestingLevel);
			printf(type == CborArrayType ? "Array[\n" : "Map[\n");
			err = cbor_value_enter_container(it, &recursed);
			if (err)
				return err;       // parse error
			err = dumprecursive(cmdCode, &recursed, (type == CborMapType), nestingLevel + 1);
			if (err)
				return err;       // parse error
			err = cbor_value_leave_container(it, &recursed);
			if (err)
				return err;       // parse error
			indent(nestingLevel);
			printf("]");
			got_next = true;
			break;
		}

		default: {
			err = dumpelm(it, &got_next, (isMapType && (elmCount % 2)) ? 0 : nestingLevel);
			if (err)
				return err;
			if (cmdCode > 0 && nestingLevel == 1 && isMapType && !(elmCount % 2)) {
				int64_t val;
				cbor_value_get_int64(it, &val);
				char *desc = fido2GetCmdMemberDescription(cmdCode, val);
				if (desc)
					printf(" (%s)", desc);
			}
			break;
		}
		}

		if (!got_next) {
			err = cbor_value_advance_fixed(it);
			if (err)
				return err;
		}
		if (isMapType && !(elmCount % 2)) {
			printf(": ");
		} else {
			printf("\n");
		}
		elmCount++;
	}
	return CborNoError;
}

int TinyCborInit(uint8_t *data, size_t length, CborValue *cb) {
	CborParser parser;
	CborError err = cbor_parser_init(data, length, 0, &parser, cb);
    if (err)
		return err;
	
	return 0;
}

int TinyCborPrintFIDOPackage(uint8_t cmdCode, uint8_t *data, size_t length) {
	CborValue cb;
	int res;
	res = TinyCborInit(data, length, &cb);
	if (res)
		return res;
	
	
	
    CborError err = dumprecursive(cmdCode, &cb, false, 0);

	if (err) {
		fprintf(stderr, "CBOR parsing failure at offset %d: %s\n",
				cb.ptr - data, cbor_error_string(err));
		return 1;
	}	
	
	
	return 0;
}
