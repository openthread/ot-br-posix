uci set luci.add_route=command
uci set luci.add_route.name='Add a new multicast route'
uci set luci.add_route.command='/usr/lib/add-route.py'
uci set luci.add_route.param='1'

uci set luci.view_routes=command
uci set luci.view_routes.name='View existing multicast route'
uci set luci.view_routes.command='/usr/lib/view-routes.py'

uci set luci.remove_routes=command
uci set luci.remove_routes.param='1'
uci set luci.remove_routes.command='/usr/lib/remove-routes.py'
uci set luci.remove_routes.name='Remove routes with string'

uci set luci.restart_smcroute=command
uci set luci.restart_smcroute.name='Restart smcroute & apply changes'
uci set luci.restart_smcroute.command='smcroutectl restart'

uci commit
