#ifndef OTBR_DBUS_THREAD_OBJECT_HPP_
#define OTBR_DBUS_THREAD_OBJECT_HPP_

#include <string>

#include "agent/ncp_openthread.hpp"
#include "dbus/dbus_object.hpp"

namespace otbr {
namespace dbus {

class DBusAgent;

class DBusThreadObject : public DBusObject
{
public:
    DBusThreadObject(DBusConnection *                             aConnection,
                     const std::string &                          aInterfaceName,
                     ot::BorderRouter::Ncp::ControllerOpenThread *aNcp);

    otbrError Init(void) override;

private:
    void ScanHandler(DBusRequest &aRequest);

    void ReplyScanResult(DBusRequest aRequest, const std::vector<otActiveScanResult> &aResult);

    ot::BorderRouter::Ncp::ControllerOpenThread *mNcp;
};

} // namespace dbus
} // namespace otbr

#endif
