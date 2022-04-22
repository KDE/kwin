# History

We started out with one method of generting classes. We then ported to a new approach of using QtWaylandScanner to reduce a lot of boiler plate.

New classes should use the new approach.

# New Approach

A public facing PIMPL class which should inherit from QObject.
A private class that should inherit from QtWaylandServer::interface_name which is auto-generated. This will manage individual resources, handle callbacks and QString conversions.

Class Names should map to the interface name in UpperCamelCase.
Where a V1 exists in the interface name, this should be mirrored in the file and class name.

An implementation should handle all versions of a given interface, but not different interfaces which represent different versions.

(i.e zxdg_output_manager_v1 versions 1 2 and 3 would be wrapped in one class XdgOutputManagerV1Interface. A zxdg_output_manager_v2 protocol would be exposed as a new public class XdgOutputManagerV2Interface)

# Implementations

There are 3 modes of operation happening within the exported classes of KWaylandServer

The generated classes can behave in all these modes, it is up to our implementation to use the correct methods.

## Globals
e.g BlurManager

This represents an object listed by the Display class.
Use the interface_name::(wl_display*, int version) constructor within the private class to create an instance.


## Server-managed multicasting resources
e.g XdgOutput

This is where one QObject represents multiple Resources.

Use the method
```cpp
QtWaylandServer::interface_name::add(client, id, version)
```

to create a a new Resource instance managed by this object.

Use the event method with the wl_resource* overload to send events.

```cpp
for (auto resource : resourceMap())
{
    send_some_method(resource->handle, arg1, arg2);
}
```

methods to send requests to all clients.

## Client-owned Resources:

e.g BlurInterface

This is where one instance of our public class represents a single resource. Typically the lifespan of the exported class matches our resource.

In the private class use the QtWaylandServer::interface_name(wl_resource*) constructor to create a wrapper bound to a specific resource.

Use
```cpp
send_some_method(arg1, args2)
```
methods of the privateClass to send events to the resource set in the constructor


## Other hooks

`_bind_resource` is called whenever a resource is bound. This exists for all generated classes in all the operating modes

`_destroy_resource` is a hook called whenever a resource has been unbound.
Note one should not call wl_resource_destroy in this hook.

## Resource destructors

destructors (tagged with type="destructor" in the XML) are not handled specially in QtWayland it is up to the class implementation to implement the event handler and call 
```cpp
wl_resource_destroy(resource->handle)
```
in the relevant method

