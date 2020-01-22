// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SAMPLES_PUBLIC_MASTER_NOTIFICATION_TYPES_H_
#define SAMPLES_PUBLIC_MASTER_NOTIFICATION_TYPES_H_

// **
// ** NOTICE
// **
// ** The notification system is deprecated, obsolete, and is slowly being
// ** removed. See https://crbug.com/268984 and https://crbug.com/170921.
// **
// ** Please don't add any new notification types, and please help migrate
// ** existing uses of the notification types below to use the Observer and
// ** Callback patterns.
// **

namespace samples {

enum NotificationType {
  NOTIFICATION_SAMPLES_START = 0,

  // General -----------------------------------------------------------------

  // Special signal value to represent an interest in all notifications.
  // Not valid when posting a notification.
  NOTIFICATION_ALL = NOTIFICATION_SAMPLES_START,

  // NavigationController ----------------------------------------------------

  // A new pending navigation has been created. Pending entries are created
  // when the user requests the navigation. We don't know if it will actually
  // happen until it does (at this point, it will be "committed." Note that
  // renderer- initiated navigations such as link clicks will never be
  // pending.
  //
  // This notification is called after the pending entry is created, but
  // before we actually try to navigate. The source will be the
  // NavigationController that owns the pending entry, and the details
  // will be a NavigationEntry.
  NOTIFICATION_NAV_ENTRY_PENDING,

  // A new non-pending navigation entry has been created. This will
  // correspond to one NavigationController entry being created (in the case
  // of new navigations) or renavigated to (for back/forward navigations).
  //
  // The source will be the navigation controller doing the commit. The
  // details will be NavigationController::LoadCommittedDetails.
  // DEPRECATED: Use WebContentsObserver::NavigationEntryCommitted()
  NOTIFICATION_NAV_ENTRY_COMMITTED,

  // Other load-related (not from NavigationController) ----------------------

  // Corresponds to ViewHostMsg_DocumentOnLoadCompletedInMainFrame. The source
  // is the WebContents.
  // DEPRECATED: Use WebContentsObserver::DocumentOnLoadCompletedInMainFrame()
  NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,

  // A samples load is starting.  The source will be a
  // Source<NavigationController> corresponding to the tab in which the load
  // is occurring.  No details are expected for this notification.
  // DEPRECATED: Use WebContentsObserver::DidStartLoading()
  NOTIFICATION_LOAD_START,

  // A samples load has stopped. The source will be a
  // Source<NavigationController> corresponding to the tab in which the load
  // is occurring.  Details in the form of a LoadNotificationDetails object
  // are optional.
  // DEPRECATED: Use WebContentsObserver::DidStopLoading()
  NOTIFICATION_LOAD_STOP,

  // WebContents ---------------------------------------------------------------

  // This message is sent after a WebContents is disconnected from the
  // renderer process.  The source is a Source<WebContents> with a pointer to
  // the WebContents (the pointer is usable).  No details are expected.
  // DEPRECATED: This is fired in two situations: when the render process
  // crashes, in which case use WebContentsObserver::RenderProcessGone, and when
  // the WebContents is being torn down, in which case use
  // WebContentsObserver::WebContentsDestroyed()
  NOTIFICATION_WEB_SAMPLESS_DISCONNECTED,

  // This notification is sent when a WebContents is being destroyed. Any
  // object holding a reference to a WebContents can listen to that
  // notification to properly reset the reference. The source is a
  // Source<WebContents>.
  // DEPRECATED: Use WebContentsObserver::WebContentsDestroyed()
  NOTIFICATION_WEB_SAMPLESS_DESTROYED,

  // A RenderViewHost was created for a WebContents. The source is the
  // associated WebContents, and the details is the RenderViewHost
  // pointer.
  // DEPRECATED: Use WebContentsObserver::RenderViewCreated()
  NOTIFICATION_WEB_SAMPLESS_SLAVER_VIEW_HOST_CREATED,

  // Indicates that a RenderProcessHost was created and its handle is now
  // available. The source will be the RenderProcessHost that corresponds to
  // the process.
  // DEPRECATED: Use RenderProcessHostObserver::RenderProcessReady()
  NOTIFICATION_SLAVERER_PROCESS_CREATED,

  // Indicates that a RenderProcessHost is destructing. The source will be the
  // RenderProcessHost that corresponds to the process.
  // DEPRECATED: Use RenderProcessHostObserver::RenderProcessHostDestroyed()
  NOTIFICATION_SLAVERER_PROCESS_TERMINATED,

  // Indicates that a render process was closed (meaning it exited, but the
  // RenderProcessHost might be reused).  The source will be the corresponding
  // RenderProcessHost.  The details will be a ChildProcessTerminationInfo
  // struct. This may get sent along with SLAVERER_PROCESS_TERMINATED.
  // DEPRECATED: Use RenderProcessHostObserver::RenderProcessExited()
  NOTIFICATION_SLAVERER_PROCESS_CLOSED,

  // Indicates that a RenderWidgetHost has become unresponsive for a period of
  // time. The source will be the RenderWidgetHost that corresponds to the
  // hung view, and no details are expected.
  NOTIFICATION_SLAVER_WIDGET_HOST_HANG,

  // This is sent when a RenderWidgetHost is being destroyed. The source is
  // the RenderWidgetHost, the details are not used.
  // DEPRECATED: Use RenderWidgetHostObserver::RenderWidgetHostDestroyed()
  NOTIFICATION_SLAVER_WIDGET_HOST_DESTROYED,

  // Sent after the renderer has updated visual properties on the main thread
  // and committed the change on the compositor thread. The source is the
  // RenderWidgetHost, the details are not used.
  NOTIFICATION_SLAVER_WIDGET_HOST_DID_UPDATE_VISUAL_PROPERTIES,

  // Indicates a RenderWidgetHost has been hidden or restored. The source is
  // the RWH whose visibility changed, the details is a bool set to true if
  // the new state is "visible."
  //
  // DEPRECATED:
  // Use RenderWidgetHostObserver::RenderWidgetHostVisibilityChanged()
  NOTIFICATION_SLAVER_WIDGET_VISIBILITY_CHANGED,

  // The focused element inside a page has changed.  The source is the
  // RenderViewHost. The details is a Details<const bool> that indicates whether
  // or not an editable node was focused.
  NOTIFICATION_FOCUS_CHANGED_IN_PAGE,

  // Notification from WebContents that we have received a response from the
  // renderer in response to a dom automation controller action. The source is
  // the RenderViewHost, and the details is a string with the response.
  NOTIFICATION_DOM_OPERATION_RESPONSE,

  // Custom notifications used by the embedder should start from here.
  NOTIFICATION_SAMPLES_END,
};

}  // namespace samples

// **
// ** NOTICE
// **
// ** The notification system is deprecated, obsolete, and is slowly being
// ** removed. See https://crbug.com/268984 and https://crbug.com/170921.
// **
// ** Please don't add any new notification types, and please help migrate
// ** existing uses of the notification types below to use the Observer and
// ** Callback patterns.
// **

#endif  // SAMPLES_PUBLIC_MASTER_NOTIFICATION_TYPES_H_
