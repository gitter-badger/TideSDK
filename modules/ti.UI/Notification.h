/*
 * Copyright (c) 2010 Appcelerator, Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef Notification_h
#define Notification_h

#include <tide/tide.h>

#ifdef OS_LINUX
typedef struct _NotifyNotification NotifyNotification;
typedef NotifyNotification* PlatformNotification;
#elif defined(OS_WIN32)
typedef long PlatformNotification;
#elif defined(OS_OSX)
// Unused.
typedef char PlatformNotification;
#endif

namespace ti {

class Notification : public KAccessorObject {
public:
    Notification();
    ~Notification();

    void Configure(ObjectRef properties);

    static bool InitializeImpl();
    static void ShutdownImpl();
    void CreateImpl();
    void DestroyImpl();
    bool ShowImpl();
    bool HideImpl();

private:
    void _SetTitle(const ValueList& args, ValueRef result);
    void _SetMessage(const ValueList& args, ValueRef result);
    void _SetIcon(const ValueList& args, ValueRef result);
    void _SetTimeout(const ValueList& args, ValueRef result);
    void _Show(const ValueList& args, ValueRef result);
    void _Hide(const ValueList& args, ValueRef result);

    std::string title;
    std::string message;
    std::string iconURL;
    int timeout;
    PlatformNotification notification;
};

typedef AutoPtr<Notification> AutoNotification;

} // namespace ti

#endif
