/**
 * Appcelerator Kroll - licensed under the Apache Public License 2
 * see LICENSE in the root folder for details on the license.
 * Copyright (c) 2008 Appcelerator, Inc. All Rights Reserved.
 */
#include "ruby_module.h"
#include <string>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace tide
{
	VALUE RubyUtils::ObjectClass = Qnil;
	VALUE RubyUtils::MethodClass = Qnil;
	VALUE RubyUtils::ListClass = Qnil;

	bool RubyUtils::KindOf(VALUE value, VALUE klass)
	{
		return rb_obj_is_kind_of(value, klass) == Qtrue;
	}

	ValueRef RubyUtils::ToTideValue(VALUE value)
	{
		ValueRef kvalue = Value::Undefined;

		int t = TYPE(value);
		if (T_NIL == t)
		{
			kvalue = Value::Null;
		}
		else if (T_FIXNUM == t || T_BIGNUM == t || T_FLOAT == t)
		{
			kvalue = Value::NewDouble(NUM2DBL(value));
		}
		else if (T_TRUE == t)
		{
			kvalue = Value::NewBool(true);
		}
		else if (T_FALSE == t)
		{
			kvalue = Value::NewBool(false);
		}
		else if (T_STRING == t)
		{
			kvalue = Value::NewString(StringValuePtr(value));
		}
		else if (T_SYMBOL == t)
		{
			kvalue = Value::NewString(rb_id2name(SYM2ID(value)));
		}
		else if (T_OBJECT == t)
		{
			ObjectRef kobj = new KRubyObject(value);
			kvalue = Value::NewObject(kobj);
		}
		else if (T_STRUCT == t)
		{
			ObjectRef kobj = new KRubyObject(value);
			kvalue = Value::NewObject(kobj);
		}
		else if (T_HASH == t)
		{
			ObjectRef kobj = new KRubyHash(value);
			kvalue = Value::NewObject(kobj);
		}
		else if (T_ARRAY == t)
		{
			ListRef List = new KRubyList(value);
			kvalue = Value::NewList(List);
		}
		else if (T_DATA == t && ObjectClass != Qnil && KindOf(value, ObjectClass))
		{
			ValueRef* kval = NULL;
			Data_Get_Struct(value, ValueRef, kval);
			kvalue = Value::NewObject((*kval)->ToObject());
		}
		else if (T_DATA == t && MethodClass != Qnil && KindOf(value, MethodClass))
		{
			ValueRef* kval = NULL;
			Data_Get_Struct(value, ValueRef, kval);
			kvalue = Value::NewMethod((*kval)->ToMethod());
		}
		else if (T_DATA == t && ListClass != Qnil && KindOf(value, ListClass))
		{
			ValueRef* kval = NULL;
			Data_Get_Struct(value, ValueRef, kval);
			kvalue = Value::NewList((*kval)->ToList());
		}
		else if (T_DATA == t && KindOf(value, rb_cMethod))
		{
			MethodRef method = new KRubyMethod(value);
			return Value::NewMethod(method);
		}
		else if (T_DATA == t && KindOf(value, rb_cProc))
		{
			MethodRef method = new KRubyMethod(value);
			return Value::NewMethod(method);
		}
		else if (T_DATA == t)
		{
			ObjectRef object = new KRubyObject(value);
			return Value::NewObject(object);
		}

		return kvalue;
	}

	VALUE RubyUtils::ToRubyValue(ValueRef value)
	{
		if (value->IsNull() || value->IsUndefined())
		{
			return Qnil;
		}
		if (value->IsBool())
		{
			return value->ToBool() ? Qtrue : Qfalse;
		}
		else if (value->IsNumber())
		{
			return rb_float_new(value->ToNumber());
		}
		else if (value->IsString())
		{
			return rb_str_new2(value->ToString());
		}
		else if (value->IsObject())
		{
			AutoPtr<KRubyObject> ro = value->ToObject().cast<KRubyObject>();
			if (!ro.isNull())
				return ro->ToRuby();

			AutoPtr<KRubyHash> rh = value->ToObject().cast<KRubyHash>();
			if (!rh.isNull())
				return rh->ToRuby();

			return RubyUtils::ObjectToRubyValue(value);
		}
		else if (value->IsMethod())
		{
			AutoPtr<KRubyMethod> rm = value->ToMethod().cast<KRubyMethod>();
			if (!rm.isNull())
				return rm->ToRuby();
			else
				return RubyUtils::MethodToRubyValue(value);
		}
		else if (value->IsList())
		{
			AutoPtr<KRubyList> rl = value->ToList().cast<KRubyList>();
			if (!rl.isNull())
				return rl->ToRuby();
			else
				return RubyUtils::ListToRubyValue(value);
		}
		return Qnil;
	}

	static VALUE RubyObjectMethods(VALUE self)
	{
		ValueRef* value = NULL;
		Data_Get_Struct(self, ValueRef, value);
		ObjectRef object = (*value)->ToObject();

		VALUE* args = NULL;
		VALUE methods = rb_call_super(0, args);

		SharedStringList props = object->GetPropertyNames();
		for (unsigned int i = 0; i < props->size(); i++)
		{
			SharedString prop_name = props->at(i);
			rb_ary_push(methods, rb_str_new2(prop_name->c_str()));
		}
		return methods;
	}

	VALUE RubyUtils::GenericMethodCall(MethodRef method, VALUE args)
	{
		ValueList kargs;
		for (int i = 0; i < RARRAY_LEN(args); i++)
		{
			VALUE rarg = rb_ary_entry(args, i);
			ValueRef arg = RubyUtils::ToTideValue(rarg);
			Value::Unwrap(arg);
			kargs.push_back(arg);
		}

		try
		{
			ValueRef result = method->Call(kargs);
			return RubyUtils::ToRubyValue(result);
		}
		catch (ValueException& e)
		{
			// TODO: Eventually wrap these up in a special exception
			// class so that we can unwrap them into ValueExceptions again
			SharedString ss = e.DisplayString();
			rb_raise(rb_eStandardError, ss->c_str());
			return Qnil;
		}
	}

	// A :method method for pulling methods off of Objects in Ruby
	static VALUE RubyObjectMethod(int argc, VALUE *argv, VALUE self)
	{
		ValueRef* dval = NULL;
		Data_Get_Struct(self, ValueRef, dval);
		ObjectRef object = (*dval)->ToObject();

		// TODO: We should raise an exception instead
		if (object.isNull())
			return Qnil;
		if (argc < 1)
			return Qnil;

		VALUE meth_name = argv[0];
		const char* name = rb_id2name(SYM2ID(meth_name));
		ValueRef v = object->Get(name);
		if (v->IsMethod())
		{
			return RubyUtils::ToRubyValue(v);
		}
		else
		{
			return Qnil;
		}
	}

	// A :method_missing method for finding Object properties in Ruby
	static VALUE RubyObjectMethodMissing(int argc, VALUE *argv, VALUE self)
	{
		ValueRef* dval = NULL;
		Data_Get_Struct(self, ValueRef, dval);
		ObjectRef object = (*dval)->ToObject();

		// TODO: We should raise an exception instead
		if (object.isNull())
			return Qnil;

		// This is the same error that ruby throws
		if (argc == 0 || !SYMBOL_P(argv[0]))
		{
			rb_raise(rb_eArgError, "no id given");
		}

		// We need to determine the method that was invoked:
		// store the method name and arguments in separate variables
		VALUE r_name, args;
		rb_scan_args(argc, argv, "1*", &r_name, &args);
		const char* name = rb_id2name(SYM2ID(r_name));

		// Check if this is an assignment
		ValueRef value = object->Get(name);
		if (name[strlen(name) - 1] == '=' && argc > 1)
		{
			char* mod_name = strdup(name);
			mod_name[strlen(mod_name) - 1] = '\0';
			value = RubyUtils::ToTideValue(argv[1]);
			object->Set(mod_name, value);
			free(mod_name);
			return argv[1];
		}
		else if (value->IsUndefined()) // raise a method missing error
		{
			VALUE selfString = rb_obj_as_string(self);
			rb_raise(rb_eNoMethodError, "undefined method `%s' for %s",
				name, StringValueCStr(selfString));
		}
		else if (value->IsMethod()) // actually call a method
		{
			return RubyUtils::GenericMethodCall(value->ToMethod(), args);
		}
		else // Plain old access
		{
			return RubyUtils::ToRubyValue(value);
		}

		return 0;
	}

	// A :responds_to? method for finding Object properties in Ruby
	static VALUE RubyObjectRespondTo(int argc, VALUE *argv, VALUE self)
	{
		ValueRef* dval = NULL;
		Data_Get_Struct(self, ValueRef, dval);
		ObjectRef object = (*dval)->ToObject();
		VALUE mid, priv; // We ignore the priv argument

		rb_scan_args(argc, argv, "11", &mid, &priv);
		const char* name = rb_id2name(rb_to_id(mid));
		ValueRef value = object->Get(name);
		return value->IsUndefined() ? Qfalse : Qtrue;
	}

	static void RubyObjectFree(void *p)
	{
		ValueRef* kval = static_cast<ValueRef*>(p);
		delete kval;
	}

	static VALUE RubyMethodCall(VALUE self, VALUE args)
	{
		ValueRef* dval = NULL;
		Data_Get_Struct(self, ValueRef, dval);
		MethodRef method = (*dval)->ToMethod();

		// TODO: We should raise an exception instead
		if (method.isNull())
			return Qnil;

		return RubyUtils::GenericMethodCall(method, args);
	}

	VALUE RubyUtils::ObjectToRubyValue(ValueRef obj)
	{
		// Lazily initialize the Object wrapper class
		if (ObjectClass == Qnil)
		{
			ObjectClass = rb_define_class("RubyObject", rb_cObject);
			rb_define_method(ObjectClass, "method_missing",
				RUBY_METHOD_FUNC(RubyObjectMethodMissing), -1);
			rb_define_method(ObjectClass, "method",
				RUBY_METHOD_FUNC(RubyObjectMethod), -1);
			rb_define_method(ObjectClass, "methods",
				RUBY_METHOD_FUNC(RubyObjectMethods), 0);
			rb_define_method(ObjectClass, "respond_to?",
				RUBY_METHOD_FUNC(RubyObjectRespondTo), -1);
		}

		VALUE wrapper = Data_Wrap_Struct(ObjectClass, 0, RubyObjectFree, new ValueRef(obj));
		rb_obj_call_init(wrapper, 0, 0);
		return wrapper;
	}

	VALUE RubyUtils::MethodToRubyValue(ValueRef obj)
	{
		// Lazily initialize the Method wrapper class
		if (MethodClass == Qnil)
		{
			MethodClass = rb_define_class("RubyMethod", rb_cObject);
			rb_define_method(MethodClass, "method_missing",
				RUBY_METHOD_FUNC(RubyObjectMethodMissing), -1);
			rb_define_method(MethodClass, "method",
				RUBY_METHOD_FUNC(RubyObjectMethod), -1);
			rb_define_method(MethodClass, "methods",
				RUBY_METHOD_FUNC(RubyObjectMethods), 0);
			rb_define_method(MethodClass, "respond_to?",
				RUBY_METHOD_FUNC(RubyObjectRespondTo), -1);
			rb_define_method(MethodClass, "call",
				RUBY_METHOD_FUNC(RubyMethodCall), -2);
		}

		VALUE wrapper = Data_Wrap_Struct(MethodClass, 0, RubyObjectFree, new ValueRef(obj));
		rb_obj_call_init(wrapper, 0, 0);
		return wrapper;
	}

	static VALUE RubyListGetElt(int argc, VALUE *argv, VALUE self)
	{
		ValueRef* dval = NULL;
		Data_Get_Struct(self, ValueRef, dval);
		ListRef list = (*dval)->ToList();

		// TODO: We should raise an exception instead
		if (list.isNull() || argc < 1)
			return Qnil;

		int idx = -1;
		if (TYPE(argv[0]) != T_FIXNUM || ((idx = NUM2INT(argv[0])) < 0))
			return Qnil;

		if (idx >= 0 && idx < (int) list->Size())
		{
			ValueRef v = list->At(idx);
			return RubyUtils::ToRubyValue(v);
		}
		else
		{
			return Qnil;
		}
	}

	static VALUE RubyListSetElt(int argc, VALUE *argv, VALUE self)
	{
		ValueRef* dval = NULL;
		Data_Get_Struct(self, ValueRef, dval);
		ListRef List = (*dval)->ToList();

		// TODO: We should raise an exception instead
		if (List.isNull() || argc < 2)
			return Qnil;
		// TODO: Maybe we should raise an exception instead
		if (TYPE(argv[0]) != T_FIXNUM)
			return Qnil;

		int idx = NUM2INT(argv[0]);
		if (idx < 0)
			return Qnil;

		ValueRef value = RubyUtils::ToTideValue(argv[1]);
		List->SetAt(idx, value);

		return argv[1];
	}

	static VALUE RubyListLength(int argc, VALUE *argv, VALUE self)
	{
		ValueRef* dval = NULL;
		Data_Get_Struct(self, ValueRef, dval);
		ListRef List = (*dval)->ToList();

		// TODO: We should raise an exception instead
		if (List.isNull())
			return Qnil;

		if (argc > 0)
		{
			rb_raise(rb_eNoMethodError, "wrong number of arguments (%d for 0)", argc);
			return Qnil;
		}
		else
		{
			return INT2NUM(List->Size());
		}
	}

	static VALUE RubyListEach(VALUE self)
	{
		ValueRef* dval = NULL;
		Data_Get_Struct(self, ValueRef, dval);
		ListRef list = (*dval)->ToList();

		if (list.isNull() || !rb_block_given_p())
			return Qnil;

		for (unsigned int i = 0; i < list->Size(); i++)
			rb_yield(RubyUtils::ToRubyValue(list->At(i)));

		return self;
	}

	static VALUE RubyListCollect(VALUE self)
	{
		ValueRef* dval = NULL;
		Data_Get_Struct(self, ValueRef, dval);
		ListRef list = (*dval)->ToList();

		if (list.isNull() || !rb_block_given_p())
			return Qnil;

		VALUE resultArray = rb_ary_new();
		for (unsigned int i = 0; i < list->Size(); i++)
			rb_ary_push(resultArray, rb_yield(RubyUtils::ToRubyValue(list->At(i))));

		return resultArray;
	}

	VALUE RubyUtils::ListToRubyValue(ValueRef obj)
	{
		// Lazily initialize the Method wrapper class
		if (ListClass == Qnil)
		{
			ListClass = rb_define_class("RubyList", rb_cObject);
			rb_define_method(ListClass, "method_missing",
				RUBY_METHOD_FUNC(RubyObjectMethodMissing), -1);
			rb_define_method(ListClass, "method",
				RUBY_METHOD_FUNC(RubyObjectMethod), -1);
			rb_define_method(ListClass, "methods",
				RUBY_METHOD_FUNC(RubyObjectMethods), 0);
			rb_define_method(ListClass, "respond_to?",
				RUBY_METHOD_FUNC(RubyObjectRespondTo), -1);
			rb_define_method(ListClass, "[]",
				RUBY_METHOD_FUNC(RubyListGetElt), -1);
			rb_define_method(ListClass, "[]=",
				RUBY_METHOD_FUNC(RubyListSetElt), -1);
			rb_define_method(ListClass, "length",
				RUBY_METHOD_FUNC(RubyListLength), -1);
			rb_define_method(ListClass, "each",
				RUBY_METHOD_FUNC(RubyListEach), 0);
			rb_define_method(ListClass, "collect",
				RUBY_METHOD_FUNC(RubyListCollect), 0);
			rb_define_method(ListClass, "map",
				RUBY_METHOD_FUNC(RubyListCollect), 0);
		}

		VALUE wrapper = Data_Wrap_Struct(ListClass, 0, RubyObjectFree, new ValueRef(obj));
		rb_obj_call_init(wrapper, 0, 0);
		return wrapper;
	}

	ValueException RubyUtils::GetException()
	{
		VALUE e = rb_gv_get("$!");
		ValueRef v = RubyUtils::ToTideValue(e);
		return ValueException(v);
	}
}
