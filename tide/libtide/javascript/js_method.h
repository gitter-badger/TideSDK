/**
 * Appcelerator Kroll - licensed under the Apache Public License 2
 * see LICENSE in the root folder for details on the license.
 * Copyright (c) 2008 Appcelerator, Inc. All Rights Reserved.
 */

#ifndef _JS_METHOD_H_
#define _JS_METHOD_H_

#include "javascript_module.h"

#include <vector>
#include <string>
#include <map>

namespace tide
{
	class TIDE_API KKJSMethod : public Method
	{
		public:
		KKJSMethod(JSContextRef, JSObjectRef, JSObjectRef);
		~KKJSMethod();

		virtual void Set(const char *name, ValueRef value);
		virtual ValueRef Get(const char *name);
		ValueRef Call(JSObjectRef thisObject, const ValueList& args);
		virtual ValueRef Call(const ValueList& args);
		virtual ValueRef Call(ObjectRef thisObject, const ValueList& args);
		virtual SharedStringList GetPropertyNames();
		virtual bool HasProperty(const char* name);
		virtual bool Equals(ObjectRef);

		virtual bool SameContextGroup(JSContextRef c);
		JSObjectRef GetJSObject();

		protected:
		JSGlobalContextRef context;
		JSObjectRef jsobject;
		JSObjectRef thisObject;
		AutoPtr<KKJSObject> object;

		private:
		DISALLOW_EVIL_CONSTRUCTORS(KKJSMethod);
	};
}

#endif