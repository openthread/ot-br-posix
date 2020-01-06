#ifndef OTBR_DBUS_AGENT_HPP_
#define OTBR_DBUS_AGENT_HPP_

#include <functional>
#include <string>
#include <sys/select.h>

#include "dbus/dbus_message_helper.hpp"
#include "dbus/dbus_object.hpp"
#include "dbus/dbus_resources.hpp"
#include "dbus/dbus_thread_object.hpp"

#include "agent/ncp_openthread.hpp"

namespace otbr {
namespace dbus {

class DBusAgent
{
public:
    DBusAgent(const std::string &aInterfaceName, ot::BorderRouter::Ncp::ControllerOpenThread *aNcp);

    otbrError Init(void);

    void MainLoop(void);

    /**
     * This method performs the DTLS processing.
     *
     * @param[in]       aMainloop   A reference to OpenThread mainloop context.
     *
     */
    void UpdateFdSet(fd_set &        aReadFdSet,
                     fd_set &        aWriteFdSet,
                     fd_set &        aErrorFdSet,
                     int &           aMaxFd,
                     struct timeval &aTimeOut);

    void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet);

private:
    static const struct timeval kPollTimeout;

    std::string                                                            mInterfaceName;
    std::unique_ptr<DBusThreadObject>                                      mThreadObject;
    std::unique_ptr<DBusConnection, std::function<void(DBusConnection *)>> mConnection;
    ot::BorderRouter::Ncp::ControllerOpenThread *                          mNcp;
};

} // namespace dbus
} // namespace otbr

#endif // OTBR_DBUS_AGENT_HPP_
