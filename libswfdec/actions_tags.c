#include <ctype.h>
#include <string.h>
#include <math.h>

#include "swfdec_internal.h"
#include "js/jsfun.h"

#define TAG_A		context->tag_argv[context->tag_argc + 0]
#define TAG_B		context->tag_argv[context->tag_argc + 1]
#define TAG_C		context->tag_argv[context->tag_argc + 2]

static void
action_goto_frame (SwfdecActionContext * context)
{
  int frame;

  frame = swfdec_bits_get_u16 (&context->bits);

  /* FIXME hack */
  context->s->parse_sprite_seg->frame_index = frame;
  if (context->s->parse_sprite_seg->frame_index < 0) {
    context->s->parse_sprite_seg->frame_index = 0;
  }
}

static void
action_get_url (SwfdecActionContext * context)
{
  char *url;
  char *target;

  url = swfdec_bits_get_string (&context->bits);
  target = swfdec_bits_get_string (&context->bits);

  SWFDEC_DEBUG ("get_url %s %s", url, target);

  if (context->s->url) g_free (context->s->url);
  context->s->url = url;
  
  g_free(target);
}

static void
action_next_frame (SwfdecActionContext * context)
{
  context->s->parse_sprite_seg->frame_index++;
}

static void
action_previous_frame (SwfdecActionContext * context)
{
  context->s->parse_sprite_seg->frame_index--;
}

static void
action_play (SwfdecActionContext * context)
{
  context->s->parse_sprite_seg->stopped = FALSE;
}

static void
action_stop (SwfdecActionContext * context)
{
  context->s->parse_sprite_seg->stopped = TRUE;
}

static void
action_toggle_quality (SwfdecActionContext * context)
{
  /* ignored */
}

static void
action_stop_sounds (SwfdecActionContext * context)
{
  /* FIXME */
  SWFDEC_WARNING ("stop_sounds unimplemented");
}

static void
action_wait_for_frame (SwfdecActionContext * context)
{
  int frame;
  int skip;

  frame = swfdec_bits_get_u16 (&context->bits);
  skip = swfdec_bits_get_u8 (&context->bits);

  if (TRUE) {
    /* frame is always loaded */
    context->skip = 0;
  } else {
    context->skip = skip;
  }
}

static void
action_set_target (SwfdecActionContext * context)
{
  char *target;

  target = swfdec_bits_get_string (&context->bits);

  SWFDEC_WARNING ("set_target unimplemented");

  g_free(target);
}

static void
action_go_to_label (SwfdecActionContext * context)
{
  char *label;

  label = swfdec_bits_get_string (&context->bits);

  SWFDEC_WARNING ("go_to_label unimplemented");

  g_free(label);
}

static void
action_push (SwfdecActionContext * context)
{
  int type, number;
  JSString *string;

  while (context->bits.ptr < context->bits.end) {
  jsval val = JSVAL_VOID;

  type = swfdec_bits_get_u8 (&context->bits);
  switch (type) {
    case 0: /* string */
      string = JS_NewStringCopyZ (context->jscx,
        swfdec_bits_get_string (&context->bits));
      val = STRING_TO_JSVAL(string);
      break;
    case 1: /* float */
      {
        union {
          float f;
          guint32 i;
        } x;

        /* FIXME check endianness */
        x.i = swfdec_bits_get_u32 (&context->bits);
        JS_NewDoubleValue (context->jscx, x.f, &val);
      }
      break;
    case 2: /* null */
      val = JSVAL_NULL;
      break;
    case 3: /* undefined */
      val = JSVAL_VOID;
      break;
    case 4: /* register number */
      {
        CallFrame *frame;
        int reg;
        reg = swfdec_bits_get_u8 (&context->bits);
        frame = g_queue_peek_head (context->call_stack);
        if (frame->is_function2) {
          JS_GetElement (context->jscx, frame->registers, reg, &val);
        } else if (reg >= 0 && reg < 4) {
          JS_GetElement (context->jscx, context->registers, reg, &val);
        }
      }
      break;
    case 5: /* boolean */
      val = BOOLEAN_TO_JSVAL(swfdec_bits_get_u8 (&context->bits) != 0);
      break;
    case 6: /* double */
      {
        union {
          double f;
          guint32 i[2];
        } x;

        /* FIXME check endianness */
        x.i[0] = swfdec_bits_get_u32 (&context->bits);
        x.i[1] = swfdec_bits_get_u32 (&context->bits);
        JS_NewDoubleValue (context->jscx, x.f, &val);
      }
      break;
    case 7: /* int32 */
      number = swfdec_bits_get_u32 (&context->bits);
      if (INT_FITS_IN_JSVAL (number)) {
        val = INT_TO_JSVAL(number);
      } else {
        JS_NewDoubleValue (context->jscx, number, &val);
      }
      break;
    case 8: /* constant8 */
      {
	ConstantPool *pool = context->constantpool;
        int i;
        i = swfdec_bits_get_u8 (&context->bits);
        if (pool && i < pool->n_constants) {
          string = JS_NewStringCopyZ (context->jscx, pool->constants[i]);
          val = STRING_TO_JSVAL(string);
        } else {
          SWFDEC_ERROR ("request for constant outside constant pool");
          context->error = 1;
        }
      }
      break;
    case 9: /* constant16 */
      {
	ConstantPool *pool = context->constantpool;
        int i;
        i = swfdec_bits_get_u16 (&context->bits);
        if (pool && i < pool->n_constants) {
          string = JS_NewStringCopyZ (context->jscx, pool->constants[i]);
          val = STRING_TO_JSVAL(string);
        } else {
          SWFDEC_ERROR ("request for constant outside constant pool");
          context->error = 1;
        }
      }
      break;
    default:
      SWFDEC_ERROR("illegal type");
  }

  stack_push (context, val);
  }
}

static void
action_pop (SwfdecActionContext * context)
{
  stack_pop (context);
}

static void
action_binary_op (SwfdecActionContext * context)
{
  double a, b;

  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);
  JS_ValueToNumber (context->jscx, TAG_A, &a);
  JS_ValueToNumber (context->jscx, TAG_B, &b);

  /* FIXME: need to treat the bit operators as integers? */

  switch (context->action) {
    case 0x0a:
      JS_NewDoubleValue (context->jscx, b + a, &TAG_A);
      break;
    case 0x0b:
      JS_NewDoubleValue (context->jscx, b - a, &TAG_A);
      break;
    case 0x0c:
      JS_NewDoubleValue (context->jscx, b * a, &TAG_A);
      break;
    case 0x0d:
      JS_NewDoubleValue (context->jscx, b / a, &TAG_A);
      break;
    case 0x0e:
      TAG_A = BOOLEAN_TO_JSVAL(b == a);
      break;
    case 0x0f:
      TAG_A = BOOLEAN_TO_JSVAL(b < a);
      break;
    case 0x10:
      TAG_A = BOOLEAN_TO_JSVAL(b && a);
      break;
    case 0x11:
      TAG_A = BOOLEAN_TO_JSVAL(b ||a);
      break;
    case 0x3f:
      JS_NewDoubleValue (context->jscx, (int)b % (int)a, &TAG_A);
      break;
    case 0x60:
      JS_NewDoubleValue (context->jscx, (int)b & (int)a, &TAG_A);
      break;
    case 0x61:
      JS_NewDoubleValue (context->jscx, (int)b | (int)a, &TAG_A);
      break;
    case 0x62:
      JS_NewDoubleValue (context->jscx, (int)b ^ (int)a, &TAG_A);
      break;
    case 0x63:
      JS_NewDoubleValue (context->jscx, (int)b << (int)a, &TAG_A);
      break;
    case 0x64:
      JS_NewDoubleValue (context->jscx, (int)b >> (int)a, &TAG_A);
      break;
    case 0x65:
      JS_NewDoubleValue (context->jscx, (unsigned int)b >> (int)a, &TAG_A);
      break;
    default:
      SWFDEC_ERROR ("should not be reached");
      break;
  }

  stack_push (context, TAG_A);
}

static void
action_not (SwfdecActionContext * context)
{
  JSBool b;

  TAG_A = stack_pop (context);
  JS_ValueToBoolean (context->jscx, TAG_A, &b);
  TAG_A = BOOLEAN_TO_JSVAL(!b);

  stack_push (context, TAG_A);
}

static void
action_string_equals (SwfdecActionContext * context)
{
  JSString *a, *b;

  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);
  a = JS_ValueToString (context->jscx, TAG_A);
  b = JS_ValueToString (context->jscx, TAG_B);

  stack_push (context, BOOLEAN_TO_JSVAL(JS_CompareStrings (a, b) == 0));
}

static void
action_string_length (SwfdecActionContext * context)
{
  JSString *a;

  TAG_A = stack_pop (context);
  a = JS_ValueToString (context->jscx, TAG_A);

  stack_push (context, INT_TO_JSVAL(JS_GetStringLength (a)));
}

static void
action_string_add (SwfdecActionContext * context)
{
  JSString *a, *b;

  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);
  a = JS_ValueToString (context->jscx, TAG_A);
  b = JS_ValueToString (context->jscx, TAG_B);

  stack_push (context, STRING_TO_JSVAL(JS_ConcatStrings (context->jscx, a, b)));
}

static void
action_string_extract (SwfdecActionContext * context)
{
  int32 a, b;
  JSString *c;
  int n;
  int count;
  int index;
  char *chars;
  JSString *new_string;

  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);
  TAG_C = stack_pop (context);
  a = JS_ValueToInt32 (context->jscx, TAG_A, &a);
  b = JS_ValueToInt32 (context->jscx, TAG_B, &b);
  c = JS_ValueToString (context->jscx, TAG_C);

  n = JS_GetStringLength (c);
  count = a;
  index = b;
  if (count < 0) count = 0;
  if (index < 0) index = 0;
  if (index > n) index = n;
  if (count > n - index) count = n - index;

  chars = JS_GetStringBytes(c);
  new_string = JS_NewStringCopyN(context->jscx, chars + index, count);

  stack_push (context, STRING_TO_JSVAL(new_string));
}

static void
action_string_less (SwfdecActionContext * context)
{
  JSString *a, *b;

  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);
  a = JS_ValueToString (context->jscx, TAG_A);
  b = JS_ValueToString (context->jscx, TAG_B);

  stack_push (context, BOOLEAN_TO_JSVAL (JS_CompareStrings(a, b) < 0));
}

static void
action_mb_string_length (SwfdecActionContext * context)
{
  JSString *a;

  TAG_A = stack_pop (context);
  a = JS_ValueToString (context->jscx, TAG_A);

  stack_push (context, INT_TO_JSVAL(JS_GetStringLength (a)));
}

static void
action_mb_string_extract (SwfdecActionContext * context)
{
  int32 a, b;
  JSString *c;
  int n;
  int count;
  int index;
  jschar *chars;
  JSString *new_string;

  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);
  TAG_C = stack_pop (context);
  a = JS_ValueToInt32 (context->jscx, TAG_A, &a);
  b = JS_ValueToInt32 (context->jscx, TAG_B, &b);
  c = JS_ValueToString (context->jscx, TAG_C);

  n = JS_GetStringLength (c);
  count = a;
  index = b;
  if (count < 0) count = 0;
  if (index < 0) index = 0;
  if (index > n) index = n;
  if (count > n - index) count = n - index;

  chars = JS_GetStringChars(c);
  new_string = JS_NewUCStringCopyN(context->jscx, chars + index, count);

  stack_push (context, STRING_TO_JSVAL(new_string));
}

static void
action_to_integer (SwfdecActionContext * context)
{
  jsdouble d;

  TAG_A = stack_pop (context);
  JS_ValueToNumber (context->jscx, TAG_A, &d);
  JS_NewNumberValue (context->jscx, floor(d), &TAG_A);

  stack_push (context, TAG_A);
}

static void
action_char_to_ascii (SwfdecActionContext * context)
{
  JSString *a;
  char *bytes;

  TAG_A = stack_pop (context);
  a = JS_ValueToString (context->jscx, TAG_C);
  bytes = JS_GetStringBytes (a);

  stack_push (context, INT_TO_JSVAL(bytes[0]));
}

static void
action_ascii_to_char (SwfdecActionContext * context)
{
  int32 a;
  char s[2];

  TAG_A = stack_pop (context);
  JS_ValueToInt32(context->jscx, TAG_A, &a);

  s[0] = a;
  s[1] = 0;

  stack_push (context, STRING_TO_JSVAL(JS_NewStringCopyN (context->jscx, s, 1)));
}

static void
action_mb_char_to_ascii (SwfdecActionContext * context)
{
  /*ActionVal *a;

  TAG_A = stack_pop (context);
  action_val_convert_to_string (a);

  a->number = g_utf8_get_char (a->string);
  g_free (a->string);
  a->type = ACTIONVAL_TYPE_NUMBER;

  stack_push (context, a);*/ /* FIXME */
}

static void
action_mb_ascii_to_char (SwfdecActionContext * context)
{
  /*ActionVal *a;
  char s[6];
  int len;

  TAG_A = stack_pop (context);
  action_val_convert_to_number (a);

  len = g_unichar_to_utf8 (a->number, s);

  a->type = ACTIONVAL_TYPE_STRING;
  a->string = g_strndup(s,len);

  stack_push (context, a);*/ /* FIXME */
}

static void
action_jump (SwfdecActionContext *context)
{
  int offset;

  offset = swfdec_bits_get_s16 (&context->bits);

  if (pc_is_valid (context, context->pc + offset) == SWF_OK) {
    context->pc += offset;
  } else {
    SWFDEC_ERROR ("bad jump of %d", offset);
  }
}

static void
action_if (SwfdecActionContext *context)
{
  JSBool a;
  int offset;

  offset = swfdec_bits_get_s16 (&context->bits);

  TAG_A = stack_pop (context);
  JS_ValueToBoolean (context->jscx, TAG_A, &a);

  if (a) {
    if (pc_is_valid (context, context->pc + offset) == SWF_OK) {
      context->pc += offset;
    } else {
      SWFDEC_ERROR ("bad branch of %d", offset);
    }
  }
}

static void
action_call (SwfdecActionContext *context)
{
  TAG_A = stack_pop (context);
  SWFDEC_ERROR ("action call unimplemented");
}

static void
action_get_variable (SwfdecActionContext *context)
{
  JSString *a;
  char *varname;

  TAG_A = stack_pop (context);
  a = JS_ValueToString (context->jscx, TAG_A);
  varname = JS_GetStringBytes (a);

  /* FIXME: Is this the best way to deal with _global? */
  if (strcmp (varname, "_global") == 0) {
    TAG_A = OBJECT_TO_JSVAL(context->global);
  } else if (strcmp (varname, "this") == 0) {
    TAG_A = context->tag_argv[-1];
  } else if (!JS_GetProperty(context->jscx, context->global, varname, &TAG_A) ||
      TAG_A == JSVAL_VOID) {
    TAG_A = JSVAL_VOID;
    SWFDEC_DEBUG ("getting uninitialized variable \"%s\"", varname);
  }

  stack_push (context, TAG_A);
}

static void
action_set_variable (SwfdecActionContext *context)
{
  char *b;

  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);
  b = JS_GetStringBytes (JS_ValueToString (context->jscx, TAG_B));

  JS_SetProperty(context->jscx, context->global, b, &TAG_A);
}

static void
action_define_local (SwfdecActionContext *context)
{
  action_set_variable (context);

  SWFDEC_WARNING ("local variables unimplemented");
}

static void
action_define_local_2 (SwfdecActionContext *context)
{
  TAG_A = stack_pop (context);

  SWFDEC_WARNING ("local variables unimplemented");
}

static void
action_get_url_2 (SwfdecActionContext *context)
{
  int send_vars;
  int reserved;
  int load_target;
  int load_vars;

  send_vars = swfdec_bits_getbits (&context->bits, 2);
  reserved = swfdec_bits_getbits (&context->bits, 4);
  load_target = swfdec_bits_getbits (&context->bits, 1);
  load_vars = swfdec_bits_getbits (&context->bits, 1);

  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);

  SWFDEC_WARNING ("action get_url_2 unimplemented");
}

static void
action_goto_frame_2 (SwfdecActionContext *context)
{
  int reserved;
  int scene_bias;
  int play;

  reserved = swfdec_bits_getbits (&context->bits, 6);
  scene_bias = swfdec_bits_getbits (&context->bits, 1);
  play = swfdec_bits_getbits (&context->bits, 1);

  TAG_A = stack_pop (context);

  SWFDEC_WARNING ("action goto_frame_2 unimplemented");
}

static void
action_set_target_2 (SwfdecActionContext *context)
{
  TAG_A = stack_pop (context);

  SWFDEC_WARNING ("action set_target_2 unimplemented");
}

static void
action_get_property (SwfdecActionContext *context)
{
  int32 a;
  char *b;

  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);
  a = JS_ValueToInt32 (context->jscx, TAG_A, &a);
  b = JS_GetStringBytes (JS_ValueToString (context->jscx, TAG_B));

  if (a >= 0 && a <= 21) {
    switch (a) {
      case 5:
        a = context->s->n_frames; /* total frames */
        break;
      case 8:
        a = context->s->width; /* width */
        break;
      case 9:
        a = context->s->height; /* height */
        break;
      case 12:
        a = context->s->n_frames; /* frames loaded */
        break;
      case 16:
        a = 1; /* high quality */
        break;
      case 19:
        a = 1; /* quality */
        break;
      case 20:
        a = context->s->mouse_x; /* mouse x */
        break;
      case 21:
        a = context->s->mouse_y; /* mouse y */
        break;
      default:
        /* FIXME need to get the property here */
        SWFDEC_WARNING ("get property unimplemented (index = %d)", a);
        break;
    }
  } else {
    SWFDEC_ERROR("property index out of range");
    context->error = 1;
  }

  stack_push (context, INT_TO_JSVAL (a));
}

static void
action_set_property (SwfdecActionContext *context)
{
  int32 b;
  char *c;

  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);
  TAG_C = stack_pop (context);
  b = JS_ValueToInt32 (context->jscx, TAG_B, &b);
  c = JS_GetStringBytes (JS_ValueToString (context->jscx, TAG_C));

  if (b >= 0 && b <= 21) {
    SWFDEC_WARNING ("set property unimplemented");
    /* FIXME need to set the property here */
  } else {
    SWFDEC_ERROR("property index out of range");
    context->error = 1;
  }
}

static void
action_clone_sprite (SwfdecActionContext *context)
{
  int32 a;

  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);
  TAG_C = stack_pop (context);
  a = JS_ValueToInt32 (context->jscx, TAG_A, &a);

  SWFDEC_WARNING ("clone sprite unimplemented");
}

static void
action_remove_sprite (SwfdecActionContext *context)
{
  TAG_A = stack_pop (context);

  SWFDEC_WARNING ("remove sprite unimplemented");
}

static void
action_start_drag (SwfdecActionContext *context)
{
  JSBool b;
  JSBool c;

  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);
  JS_ValueToBoolean (context->jscx, TAG_B, &b);
  TAG_C = stack_pop (context);
  JS_ValueToBoolean (context->jscx, TAG_C, &c);
  if (c) {
    (void)stack_pop (context);
    (void)stack_pop (context);
    (void)stack_pop (context);
    (void)stack_pop (context);
  }

  SWFDEC_WARNING ("start drag unimplemented");
}

static void
action_end_drag (SwfdecActionContext *context)
{
  SWFDEC_WARNING ("end drag unimplemented");
}

static void
action_wait_for_frame_2 (SwfdecActionContext *context)
{
  TAG_A = stack_pop (context);

  /* FIXME frame has always been loaded */
  if (1) {
    context->skip = swfdec_bits_get_u8 (&context->bits);
  }
}

static void
action_trace (SwfdecActionContext *context)
{
  char *a;

  TAG_A = stack_pop (context);
  a = JS_GetStringBytes (JS_ValueToString (context->jscx, TAG_A));

  SWFDEC_DEBUG ("trace: %s", a);
}

static void
action_get_time (SwfdecActionContext *context)
{
  /* FIXME implement */
  //a->number = swfdec_decoder_get_time ();

  stack_push (context, INT_TO_JSVAL(0));
}

static void
action_random_number (SwfdecActionContext *context)
{
  jsdouble a;

  TAG_A = stack_pop (context);
  JS_ValueToNumber (context->jscx, TAG_A, &a);

  a = g_random_int_range (0, a);
  JS_NewNumberValue (context->jscx, a, &TAG_A);

  stack_push (context, TAG_A);
}

static void
action_call_function (SwfdecActionContext *context)
{
  JSObject *root;
  JSString *a;
  int32 b;
  jsval *argv;
  jsval rval, fval;
  int i;
  JSBool ok;
  char *funcname;

  TAG_A = stack_pop (context); /* name */
  TAG_B = stack_pop (context); /* argc */
  a = JS_ValueToString (context->jscx, TAG_A);
  JS_ValueToInt32 (context->jscx, TAG_B, &b);

  funcname = JS_GetStringBytes (a);

  SWFDEC_DEBUG("Calling function %s with %d args", funcname, b);

  JS_GetProperty (context->jscx, context->global, funcname, &fval);
  if (!JSVAL_IS_OBJECT(fval) || !JS_ObjectIsFunction(context->jscx,
      JSVAL_TO_OBJECT(fval))) {
    SWFDEC_WARNING("Couldn't look up function %s", funcname);
    stack_push (context, JSVAL_VOID);
    return;
  }

  argv = g_malloc (sizeof(jsval) * b);
  for (i = 0; i < b; i++) {
    argv[i] = stack_pop (context);
  }

  root = movieclip_find(context, context->s->main_sprite_seg);
  ok = JS_CallFunctionValue (context->jscx, root, fval, b, argv,
    &rval);
  if (!ok)
    SWFDEC_WARNING("Call of function %s failed", funcname);

  g_free (argv);

  stack_push (context, rval);
}

static void
action_call_method (SwfdecActionContext *context)
{
  JSString *a;
  JSObject *b;
  JSBool ok;
  int32 c = 10000000;
  jsval *argv;
  jsval rval = JSVAL_VOID;
  char *methodname;
  int i;

  TAG_A = stack_pop (context); /* property */
  TAG_B = stack_pop (context); /* object */
  TAG_C = stack_pop (context); /* argc */
  a = JS_ValueToString (context->jscx, TAG_A);
  JS_ValueToObject (context->jscx, TAG_B, &b);
  JS_ValueToInt32 (context->jscx, TAG_C, &c);

  if (c > context->stack_top) {
    SWFDEC_ERROR ("argc is larger than stack (%d > %d)", c, context->stack_top);
    SWFDEC_ERROR ("%x", TAG_C);
    c = context->stack_top;
  }
  
  argv = g_malloc (sizeof(jsval) * c);
  for (i = 0; i < c; i++) {
    argv[i] = stack_pop (context);
  }

  if (b == NULL) {
    SWFDEC_DEBUG ("couldn't convert value 0x%x to object while trying to call "
      "method \"%s\"", TAG_B, JS_GetStringBytes (a));
    g_free (argv);
    stack_push (context, JSVAL_VOID);
    return;
  }

  if (JS_GetStringLength(a) == 0) {
    JSObject *root = movieclip_find(context, context->s->main_sprite_seg);
    JS_CallFunctionValue (context->jscx, root, TAG_B, c, argv, &rval);
  } else {
    methodname = JS_GetStringBytes (a);
    SWFDEC_DEBUG("Calling method %s of object 0x%x", methodname, b);
    ok = JS_CallFunctionName (context->jscx, b, methodname, c, argv, &rval);
    if (!ok)
      SWFDEC_WARNING("Call of method %s of object 0x%x failed", methodname, b);
  }

  g_free (argv);

  stack_push (context, rval);
}

static void
action_constant_pool (SwfdecActionContext *context)
{
  int n;
  int i;
  ConstantPool *pool;

  n = swfdec_bits_get_u16 (&context->bits);

  if (context->constantpool) {
    constant_pool_unref (context->constantpool);
  }

  pool = g_malloc (sizeof(ConstantPool));
  pool->n_constants = n;
  pool->constants = g_malloc(sizeof(char *) * n);
  pool->refcount = 1;
  for(i = 0; i < n; i++){
    pool->constants[i] = swfdec_bits_get_string (&context->bits);
  }
  context->constantpool = pool;
}

static void
action_define_function (SwfdecActionContext *context)
{
  int i;
  ScriptFunction *func;
  JSFunction *jsfunc;
  JSObject *obj;

  func = g_malloc0 (sizeof (ScriptFunction));
  if (func == NULL) {
    SWFDEC_ERROR ("out of memory");
    return;
  }

  func->name = swfdec_bits_get_string (&context->bits);
  func->num_params = swfdec_bits_get_u16 (&context->bits);
  for (i = 0; i < func->num_params; i++) {
    /* Eat up the parameter names, which are unneeded. */
    g_free(swfdec_bits_get_string (&context->bits));
  }
  func->pc = context->pc;
  func->code_size = swfdec_bits_get_u16 (&context->bits);
  func->buffer = swfdec_buffer_new_subbuffer (context->bits.buffer,
    context->pc - context->bits.buffer->data, func->code_size);
  func->constantpool = context->constantpool;
  if (func->constantpool)
    constant_pool_ref(func->constantpool);

  /* It appears that the function body is another set of actions, not data
   * inside of this action, so we move the outside pc over it.
   */
  if (pc_is_valid (context, context->pc + func->code_size) == SWF_OK) {
    context->pc += func->code_size;
  } else {
    SWFDEC_ERROR ("invalid code size %d", func->code_size);
  }

  jsfunc = JS_NewFunction (context->jscx, action_script_call,
    func->num_params + FUNCTION_TEMPORARIES, 0, NULL, func->name);
  jsfunc->priv = func;
  obj = JS_GetFunctionObject (jsfunc);
  
  if (strcmp (func->name, "") == 0) {
    stack_push (context, OBJECT_TO_JSVAL(obj));
  } else {
    jsval funcv = OBJECT_TO_JSVAL(obj);

    JS_SetProperty (context->jscx, context->global, func->name, &funcv);
  }
}

static void
action_delete_2 (SwfdecActionContext *context)
{
  TAG_A = stack_pop (context);

  SWFDEC_WARNING ("unimplemented");
}

static void
action_equals_2 (SwfdecActionContext * context)
{
  int atag, btag;

  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);

  atag = JSVAL_TAG(TAG_A);
  btag = JSVAL_TAG(TAG_B);

  if (atag == btag) {
    if (atag == JSVAL_STRING) {
      JSString *a = JSVAL_TO_STRING(TAG_A);
      JSString *b = JSVAL_TO_STRING(TAG_B);
      TAG_C = BOOLEAN_TO_JSVAL(JS_CompareStrings (a, b) == 0);
    } else if (atag == JSVAL_DOUBLE) {
      jsdouble a = *JSVAL_TO_DOUBLE(TAG_A);
      jsdouble b = *JSVAL_TO_DOUBLE(TAG_B);
      TAG_C = BOOLEAN_TO_JSVAL(a == b);
    } else {
      TAG_C = BOOLEAN_TO_JSVAL(TAG_A == TAG_B);
    }
  } else {
    if ((JSVAL_IS_NULL(TAG_A) && JSVAL_IS_VOID(TAG_B)) ||
	(JSVAL_IS_VOID(TAG_A) && JSVAL_IS_NULL(TAG_B))) {
      TAG_C = BOOLEAN_TO_JSVAL(TRUE);
    } else if (JSVAL_IS_OBJECT(TAG_B) || JSVAL_IS_OBJECT(TAG_B)) {
      SWFDEC_WARNING ("object toprimitive unimplemented");
      TAG_C = BOOLEAN_TO_JSVAL(FALSE);
    } else {
      jsdouble a, b;
      JS_ValueToNumber (context->jscx, TAG_A, &a);
      JS_ValueToNumber (context->jscx, TAG_B, &b);
      TAG_C = BOOLEAN_TO_JSVAL(a == b);
    }
  }

  stack_push (context, TAG_C);
}

static void
action_get_member (SwfdecActionContext *context)
{
  JSString *a;
  JSObject *b;
  char *membername;
  JSBool ok;

  TAG_A = stack_pop (context); /* property */
  TAG_B = stack_pop (context); /* object */
  a = JS_ValueToString (context->jscx, TAG_A);
  JS_ValueToObject (context->jscx, TAG_B, &b);

  membername = JS_GetStringBytes (a);

  if (b == NULL) {
    SWFDEC_DEBUG ("couldn't convert value 0x%x to object while getting "
      "property \"%s\"", TAG_B, membername);
    stack_push (context, JSVAL_VOID);
    return;
  }

  TAG_C = JSVAL_VOID;
  ok = JS_GetProperty (context->jscx, b, membername, &TAG_C);

  if (!ok || TAG_C == JSVAL_VOID)
    SWFDEC_DEBUG ("couldn't get property %s", membername);

  stack_push (context, TAG_C);
}

static void
action_init_object (SwfdecActionContext *context)
{
  int32 a;
  int i;

  TAG_A = stack_pop (context); /* number of properties */
  JS_ValueToInt32 (context->jscx, TAG_A, &a);

  TAG_C = OBJECT_TO_JSVAL(JS_NewObject (context->jscx, NULL, NULL, NULL));

  for (i = 0; i < a; i++) {
    JSString *str;

    TAG_A = stack_pop (context);
    TAG_B = stack_pop (context);
    str = JS_ValueToString (context->jscx, TAG_A);
    JS_SetProperty (context->jscx, JSVAL_TO_OBJECT(TAG_C),
      JS_GetStringBytes (str), &TAG_B);
  }

  stack_push (context, TAG_C);
}

static void
action_new_method (SwfdecActionContext *context)
{
  TAG_A = stack_pop (context); /* method name (or "") */
  TAG_B = stack_pop (context); /* ScriptObject (function if method == "") */
  TAG_C = stack_pop (context); /* num args */

  SWFDEC_WARNING ("unimplemented");
}

static void
action_new_object (SwfdecActionContext *context)
{
  JSString *a;
  JSObject *obj, *constructor;
  JSClass *clasp;
  char *objname;
  int32 b;
  jsval rval = JSVAL_VOID;
  jsval *argv;
  int i;

  TAG_A = stack_pop (context); /* object name */
  TAG_B = stack_pop (context); /* num args */
  a = JS_ValueToString (context->jscx, TAG_A);
  JS_ValueToInt32 (context->jscx, TAG_B, &b);

  objname = JS_GetStringBytes (a);
  /* FIXME: Is this the best way to deal with _global? */
  /* FIXME: Push get_variable to a separate function */
  if (strcmp (objname, "_global") == 0) {
    TAG_C = OBJECT_TO_JSVAL(context->global);
  } else if (strcmp (objname, "this") == 0) {
    TAG_C = context->tag_argv[-1];
  } else if (!JS_GetProperty(context->jscx, context->global, objname, &TAG_C) ||
      TAG_C == JSVAL_VOID) {
    SWFDEC_WARNING ("getting uninitialized variable \"%s\"", objname);
    stack_push (context, JSVAL_VOID);
    return;
  }

  JS_ValueToObject (context->jscx, TAG_C, &constructor);
  if (!constructor) {
    SWFDEC_WARNING ("couldn't convert variable \"%s\" (0x%x) to object",
      objname, TAG_C);
    stack_push (context, JSVAL_VOID);
    return;
  } else {
    SWFDEC_DEBUG ("creating new object from constructor \"%s\" (0x%x)",
      objname);
  }

  argv = g_malloc (sizeof(jsval) * b);
  for (i = 0; i < b; i++) {
    argv[i] = stack_pop (context);
  }

  clasp = JS_GetClass(constructor);
  if (strcmp (objname, clasp->name) == 0) {
    /* We're constructing a standard class, so use a proper constructor call. */
    argv = g_malloc (sizeof(jsval) * b);
    for (i = 0; i < b; i++) {
      argv[i] = stack_pop (context);
    }

    rval = OBJECT_TO_JSVAL(JS_ConstructObjectWithArguments (context->jscx,
        clasp, NULL, constructor, b, argv));
  } else {
    /* We're constructing something script-defined, we hope. Script-defined
     * constructors don't get found by JS_ConstructObjectWithArguments, so we
     * try to do the equivalent with a plain function call.
     */
    obj = JS_NewObject (context->jscx, NULL, NULL, constructor);

    argv = g_malloc (sizeof(jsval) * b);
    for (i = 0; i < b; i++) {
      argv[i] = stack_pop (context);
    }

    if (!JS_CallFunctionValue (context->jscx, obj, OBJECT_TO_JSVAL(constructor),
        b, argv, &rval)) {
      SWFDEC_WARNING ("couldn't call constructor");
      g_free (argv);
      return;
    }
  }

  g_free (argv);

  stack_push (context, rval);
}


static void
action_set_member (SwfdecActionContext *context)
{
  JSString *b;
  JSObject *c;
  JSBool ok;
  char *membername;

  TAG_A = stack_pop (context); /* value */
  TAG_B = stack_pop (context); /* property */
  TAG_C = stack_pop (context); /* object */
  b = JS_ValueToString (context->jscx, TAG_B);

  /* FIXME: Why is this necessary?  It seems that if we just JS_ValueToObject,
   * something wonky happens with f5as_flakes.swf and calling randomBetween.
   */
  if (JSVAL_IS_OBJECT(TAG_C)) {
    c = JSVAL_TO_OBJECT(TAG_C);
  } else {
    JS_ValueToObject (context->jscx, TAG_C, &c);
  }

  membername = JS_GetStringBytes (b);

  if (c == NULL) {
    SWFDEC_DEBUG ("couldn't convert value 0x%x to object while setting "
      "property \"%s\"", TAG_C, membername);
    return;
  }

  ok = JS_SetProperty (context->jscx, c, membername, &TAG_A);
  if (ok) {
    SWFDEC_DEBUG ("set property %s of object %p", membername, c);
  } else {
    SWFDEC_WARNING ("failed to set property %s of object %p", membername, c);
  }
}

static void
action_to_number (SwfdecActionContext * context)
{
  jsdouble a;

  TAG_A = stack_pop (context);
  JS_ValueToNumber (context->jscx, TAG_A, &a);

  TAG_A = JSVAL_VOID;
  JS_NewNumberValue (context->jscx, a, &TAG_A);

  stack_push (context, TAG_A);
}

static void
action_to_string (SwfdecActionContext * context)
{
  JSString *a;

  TAG_A = stack_pop (context);
  a = JS_ValueToString (context->jscx, TAG_A);

  stack_push (context, STRING_TO_JSVAL(a));
}

static void
action_typeof (SwfdecActionContext * context)
{
  const char *type;

  TAG_A = stack_pop (context);

  type = JS_GetTypeName (context->jscx, JS_TypeOfValue (context->jscx, TAG_A));
  stack_push (context, STRING_TO_JSVAL(JS_NewStringCopyZ (context->jscx, type)));
}

static void
action_add_2 (SwfdecActionContext * context)
{
  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);

  if (JSVAL_IS_OBJECT(TAG_A) || JSVAL_IS_OBJECT(TAG_B)) {
    SWFDEC_WARNING ("object toPrimitive unsupported");
    TAG_C = JSVAL_VOID;
  } if (JSVAL_IS_STRING(TAG_A) || JSVAL_IS_STRING(TAG_B)) {
    JSString *a, *b;

    a = JS_ValueToString (context->jscx, TAG_A);
    b = JS_ValueToString (context->jscx, TAG_B);
    TAG_C = STRING_TO_JSVAL(JS_ConcatStrings (context->jscx, b, a));
  } else {
    jsdouble a, b;

    a = JS_ValueToNumber (context->jscx, TAG_A, &a);
    b = JS_ValueToNumber (context->jscx, TAG_B, &b);
    JS_NewNumberValue (context->jscx, a + b, &TAG_C);
  }

  stack_push (context, TAG_C);
}

static void
action_less_2 (SwfdecActionContext * context)
{
  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);

  if (JSVAL_IS_STRING(TAG_A) && JSVAL_IS_STRING(TAG_B)) {
    JSString *a, *b;

    a = JSVAL_TO_STRING(TAG_A);
    b = JSVAL_TO_STRING(TAG_A);
    TAG_C = BOOLEAN_TO_JSVAL(JS_CompareStrings (b, a) < 0);
  } else {
    jsdouble a, b;

    JS_ValueToNumber (context->jscx, TAG_A, &a);
    JS_ValueToNumber (context->jscx, TAG_B, &b);
    /* FIXME: Handle NaN, +/- 0, +/- inf */
    TAG_C = BOOLEAN_TO_JSVAL(b < a);
  }

  stack_push (context, TAG_C);
}

static void
action_unary_op (SwfdecActionContext * context)
{
  jsdouble a;

  TAG_A = stack_pop (context);
  JS_ValueToNumber (context->jscx, TAG_A, &a);

  switch (context->action) {
    case 0x50:
      a++;
      break;
    case 0x51:
      a--;
      break;
    default:
      SWFDEC_ERROR ("should not be reached");
      break;
  }

  TAG_A = JSVAL_VOID;
  JS_NewNumberValue (context->jscx, a, &TAG_A);

  stack_push (context, TAG_A);
}

static void
action_push_duplicate (SwfdecActionContext * context)
{
  TAG_A = stack_pop (context);

  /* FIXME: Push duplicate of reference or value? */
  stack_push (context, TAG_A);
  stack_push (context, TAG_A);
}

static void
action_return (SwfdecActionContext * context)
{
  CallFrame *frame;

  frame = g_queue_peek_head (context->call_stack);
  frame->done = 1;
}

static void
action_swap (SwfdecActionContext * context)
{
  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);

  stack_push (context, TAG_A);
  stack_push (context, TAG_B);
}

static void
action_store_register (SwfdecActionContext * context)
{
  int reg;
  CallFrame *frame;

  reg = swfdec_bits_get_u8 (&context->bits);

  TAG_A = stack_pop (context);

  frame = g_queue_peek_head (context->call_stack);
  if (frame != NULL && frame->is_function2) {
    if (reg >= 0 && reg < 256) {
      JS_SetElement (context->jscx, frame->registers, reg, &TAG_A);
    }
  } else if (reg >= 0 && reg < 4) {
    JS_SetElement (context->jscx, context->registers, reg, &TAG_A);
  }

  stack_push (context, TAG_A);
}

static void
action_enumerate_2 (SwfdecActionContext * context)
{
  /*GList *g;

  TAG_A = stack_pop (context);
  action_val_convert_to_object (a);

  b = action_val_new ();
  b->type = ACTIONVAL_TYPE_NULL;
  stack_push (context, b);

  for (g = g_list_first (a->obj->properties); g; g = g_list_next (g)) {
    ScriptObjectProperty *prop = g->data;
    if (!(prop->flags & OBJPROP_DONTENUM)) {
      b = action_val_new ();
      b->type = ACTIONVAL_TYPE_STRING;
      b->string = g_strdup (prop->name);
      stack_push (context, b);
    }
  }

  action_val_free(a);*/ /* FIXME */
}

static void
action_greater (SwfdecActionContext * context)
{
  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);

  if (JSVAL_IS_STRING(TAG_A) && JSVAL_IS_STRING(TAG_B)) {
    JSString *a, *b;

    a = JSVAL_TO_STRING(TAG_A);
    b = JSVAL_TO_STRING(TAG_A);
    TAG_C = BOOLEAN_TO_JSVAL(JS_CompareStrings (b, a) > 0);
  } else {
    jsdouble a, b;

    JS_ValueToNumber (context->jscx, TAG_A, &a);
    JS_ValueToNumber (context->jscx, TAG_B, &b);
    /* FIXME: Handle NaN, +/- 0, +/- inf */
    TAG_C = BOOLEAN_TO_JSVAL(b > a);
  }

  stack_push (context, TAG_C);
}

static void
action_string_greater (SwfdecActionContext * context)
{
  JSString *a, *b;

  TAG_A = stack_pop (context);
  TAG_B = stack_pop (context);
  a = JS_ValueToString (context->jscx, TAG_A);
  b = JS_ValueToString (context->jscx, TAG_B);

  stack_push (context, BOOLEAN_TO_JSVAL (JS_CompareStrings(a, b) > 0));
}

static void
action_define_function_2 (SwfdecActionContext *context)
{
  int i;
  ScriptFunction *func;
  JSFunction *jsfunc;
  JSObject *obj;

  func = g_malloc0 (sizeof (ScriptFunction));
  if (func == NULL) {
    SWFDEC_ERROR ("out of memory");
    return;
  }

  func->name = swfdec_bits_get_string (&context->bits);
  func->num_params = swfdec_bits_get_u16 (&context->bits);
  func->register_count = swfdec_bits_get_u8 (&context->bits);
  func->preload_parent = swfdec_bits_getbit (&context->bits);
  func->preload_root = swfdec_bits_getbit (&context->bits);
  func->suppress_super = swfdec_bits_getbit (&context->bits);
  func->preload_super = swfdec_bits_getbit (&context->bits);
  func->suppress_args = swfdec_bits_getbit (&context->bits);
  func->preload_args = swfdec_bits_getbit (&context->bits);
  func->suppress_this = swfdec_bits_getbit (&context->bits);
  func->preload_this = swfdec_bits_getbit (&context->bits);
  (void)swfdec_bits_getbits (&context->bits, 7);
  func->preload_global = swfdec_bits_getbit (&context->bits);

  func->param_regs = g_malloc0 (func->num_params);
  for (i = 0; i < func->num_params; i++) {
    char *param_name;

    func->param_regs[i] = swfdec_bits_get_u8 (&context->bits);
    param_name = swfdec_bits_get_string (&context->bits);
    free(param_name);
  }

  func->pc = context->pc;
  func->code_size = swfdec_bits_get_u16 (&context->bits);
  func->buffer = swfdec_buffer_new_subbuffer (context->bits.buffer,
    context->pc - context->bits.buffer->data, func->code_size);
  func->is_function2 = 1;
  func->constantpool = context->constantpool;
  if (func->constantpool)
    constant_pool_ref(func->constantpool);

  /* It appears that the function body is another set of actions, not data
   * inside of this action, so we move the outside pc over it.
   */
  if (pc_is_valid (context, context->pc + func->code_size) == SWF_OK) {
    context->pc += func->code_size;
  } else {
    SWFDEC_ERROR ("invalid code size %d", func->code_size);
  }

  jsfunc = JS_NewFunction (context->jscx, action_script_call,
    func->num_params + FUNCTION_TEMPORARIES, 0, NULL, func->name);
  jsfunc->priv = func;
  obj = JS_GetFunctionObject (jsfunc);
  
  if (strcmp (func->name, "") == 0) {
    stack_push (context, OBJECT_TO_JSVAL(obj));
  } else {
    jsval funcv = OBJECT_TO_JSVAL(obj);

    JS_SetProperty (context->jscx, context->global, func->name, &funcv);
  }
}

static void
action_extends (SwfdecActionContext * context)
{
  JSObject *a, *b;

  TAG_A = stack_pop (context); /* superclass */
  JS_ValueToObject (context->jscx, TAG_A, &a);
  TAG_B = stack_pop (context); /* subclass */
  JS_ValueToObject (context->jscx, TAG_B, &b);

  /*obj = obj_new_Object();
  c = action_val_new ();
  action_val_set_to_object (c, obj);

  d = obj_get_property (a->obj, "prototype");
  if (d) {
    obj_set_property (obj, "__proto__", d,
      OBJPROP_DONTENUM | OBJPROP_DONTDELETE | OBJPROP_DONTWRITE);
  } else {
    SWFDEC_WARNING("superclass has no prototype");
  }
  obj_set_property (obj, "__constructor__", a, 0);
  obj_set_property (b->obj, "prototype", c,
    OBJPROP_DONTENUM | OBJPROP_DONTDELETE | OBJPROP_DONTWRITE);*/ /* FIXME */

  stack_push (context, TAG_A);
}

extern ActionFuncEntry swf_actions[];

ActionFuncEntry swf_actions[] = {
  { 0x0, action_return, "(end marker)" },
  /* version 3 */
  { 0x81, action_goto_frame, "ActionGoToFrame" },
  { 0x83, action_get_url, "ActionGetURL" },
  { 0x04, action_next_frame, "ActionNextFrame" },
  { 0x05, action_previous_frame, "ActionPreviousFrame" },
  { 0x06, action_play, "ActionPlay" },
  { 0x07, action_stop, "ActionStop" },
  { 0x08, action_toggle_quality, "ActionToggleQuality" },
  { 0x09, action_stop_sounds, "ActionStopSounds" },
  { 0x8a, action_wait_for_frame, "ActionWaitForFrame" },
  { 0x8b, action_set_target, "ActionSetTarget" },
  { 0x8c, action_go_to_label, "ActionGoToLabel" },
  /* version 4 */
  { 0x96, action_push, "ActionPush" },
  { 0x17, action_pop, "ActionPop" },
  { 0x0a, action_binary_op, "ActionAdd" },
  { 0x0b, action_binary_op, "ActionSubtract" },
  { 0x0c, action_binary_op, "ActionMultiply" },
  { 0x0d, action_binary_op, "ActionDivide" },
  { 0x0e, action_binary_op, "ActionEquals" },
  { 0x0f, action_binary_op, "ActionLess" },
  { 0x10, action_binary_op, "ActionAnd" },
  { 0x11, action_binary_op, "ActionOr" },
  { 0x12, action_not, "ActionNot" },
  { 0x13, action_string_equals, "ActionStringEquals" },
  { 0x14, action_string_length, "ActionStringLength" },
  { 0x21, action_string_add, "ActionStringAdd" },
  { 0x15, action_string_extract, "ActionStringExtract" },
  { 0x29, action_string_less, "ActionStringLess" },
  { 0x31, action_mb_string_length },
  { 0x35, action_mb_string_extract },
  { 0x18, action_to_integer, "ActionToInteger" },
  { 0x32, action_char_to_ascii, "ActionCharToAscii" },
  { 0x33, action_ascii_to_char, "ActionAsciiToChar" },
  { 0x36, action_mb_char_to_ascii, "ActionMBCharToAscii" },
  { 0x37, action_mb_ascii_to_char, "ActionMVAsciiToChar" },
  { 0x99, action_jump, "ActionJump" },
  { 0x9d, action_if, "ActionIf" },
  { 0x9e, action_call, "ActionCall" },
  { 0x1c, action_get_variable, "ActionGetVariable" },
  { 0x1d, action_set_variable, "ActionSetVariable" },
  { 0x9a, action_get_url_2, "ActionGetURL2" },
  { 0x9f, action_goto_frame_2, "ActionGotoFrame2" },
  { 0x20, action_set_target_2, "ActionSetTarget2" },
  { 0x22, action_get_property, "ActionGetProperty" },
  { 0x23, action_set_property, "ActionSetProperty" },
  { 0x24, action_clone_sprite, "ActionCloneSprite" },
  { 0x25, action_remove_sprite, "ActionRemoveSprite" },
  { 0x27, action_start_drag, "ActionStartDrag" },
  { 0x28, action_end_drag, "ActionEndDrag" },
  { 0x8d, action_wait_for_frame_2, "ActionWaitForFrame2" },
  { 0x26, action_trace, "ActionTrace" },
  { 0x34, action_get_time, "ActionGetTime" },
  { 0x30, action_random_number, "ActionRandomNumber" },
  /* version 5 */
  { 0x3d, action_call_function, "ActionCallFunction" },
  { 0x52, action_call_method, "ActionCallMethod" },
  { 0x88, action_constant_pool, "ActionConstantPool" },
  { 0x9b, action_define_function, "ActionDefineFunction" },
  { 0x3c, action_define_local, "ActionDefineLocal" },
  { 0x41, action_define_local_2, "ActionDefineLocal2" },
  /* { 0x3a, action_delete, "ActionDelete" }, */
  { 0x3b, action_delete_2, "ActionDelete2" },
  /* { 0x46, action_enumerate, "ActionEnumerate" }, */
  { 0x49, action_equals_2, "ActionEquals2" },
  { 0x4e, action_get_member, "ActionGetMember" },
  /* { 0x42, action_init_array }, */
  { 0x43, action_init_object, "ActionInitObject" },
  { 0x53, action_new_method, "ActionNewMethod" },
  { 0x40, action_new_object, "ActionNewObject" },
  { 0x4f, action_set_member, "ActionSetMember" },
  /* { 0x45, action_target_path, "ActionTargetPath" }, */
  /* { 0x94, action_with, "ActionWith" }, */
  { 0x4a, action_to_number, "ActionToNumber" },
  { 0x4b, action_to_string, "ActionToString" },
  { 0x44, action_typeof, "ActionTypeof" },
  { 0x47, action_add_2, "ActionAdd2" },
  { 0x48, action_less_2, "ActionLess2" },
  { 0x3f, action_binary_op, "ActionModulo" },
  { 0x60, action_binary_op, "ActionBitAnd" },
  { 0x63, action_binary_op, "ActionBitLShift" },
  { 0x61, action_binary_op, "ActionBitOr" },
  { 0x64, action_binary_op, "ActionBitRShift" },
  { 0x65, action_binary_op, "ActionBitURShift" },
  { 0x62, action_binary_op, "ActionBitXor" },
  { 0x51, action_unary_op, "ActionDecrement" },
  { 0x50, action_unary_op, "ActionIncrement" },
  { 0x4c, action_push_duplicate, "ActionPushDuplicate" },
  { 0x3e, action_return, "ActionReturn" },
  { 0x4d, action_swap, "ActionSwap" },
  { 0x87, action_store_register, "ActionStoreRegister" },

  /* version 6 */
  /* { 0x54, action_instance_of }, */
  { 0x55, action_enumerate_2, "ActionEnumerate2" },
  /* { 0x66, action_strict_equals }, */
  { 0x67, action_greater, "ActionGreater" },
  { 0x68, action_string_greater, "ActionStringGreater" },

  /* version 7 */
  { 0x8e, action_define_function_2, "ActionDefineFunction2" },
  { 0x69, action_extends, "ActionExtends" },
  /* { 0x2b, action_cast_op }, */
  /* { 0x2c, action_implements_op }, */
  /* { 0x8f, action_try }, */
  /* { 0x2a, action_throw }, */
};

ActionFuncEntry *
get_action (int action)
{
  int i;

  for (i=0;i<sizeof(swf_actions)/sizeof(swf_actions[0]);i++){
    if (swf_actions[i].action == action) {
      return swf_actions + i;
    }
  }
  return NULL;
}

