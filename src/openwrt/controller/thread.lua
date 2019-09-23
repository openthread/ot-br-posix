module("luci.controller.admin.thread", package.seeall)

function index()
	page = entry({"admin", "network", "thread"}, template("admin_thread/thread_overview"), translate("Thread"), 16)
	page.leaf = true

	page = entry({"admin", "network", "thread_state"}, call("thread_state"), nil)
	page.leaf = true

	page = entry({"admin", "network", "thread_neighbors"}, call("thread_neighbors"), nil)
	page.leaf = true

	page = entry({"admin", "network", "thread_graph"}, call("thread_graph"), nil)
	page.leaf = true

	page = entry({"admin", "network", "thread_scan"}, template("admin_thread/thread_scan"), nil)
	page.leaf = true

	page = entry({"admin", "network", "thread_create"}, template("admin_thread/thread_setting"), nil)
	page.leaf = true

	page = entry({"admin", "network", "thread_setting"}, template("admin_thread/thread_setting"), nil)
	page.leaf = true

	page = entry({"admin", "network", "thread_add"}, post("thread_add"), nil)
	page.leaf = true

	page = entry({"admin", "network", "thread_add_page"}, template("admin_thread/thread_add"), nil)
	page.leaf = true

	page = entry({"admin", "network", "thread_view"}, template("admin_thread/thread_view"), nil)
	page.leaf = true

	page = entry({"admin", "network", "thread_join"}, template("admin_thread/thread_join"), nil)
	page.leaf = true

	page = entry({"admin", "network", "joiner_remove"}, post("joiner_remove"), nil)
	page.leaf = true

	page = entry({"admin", "network", "thread_attach"}, post("thread_attach"), nil)
	page.leaf = true

	page = entry({"admin", "network", "thread_add_joiner"}, post("thread_add_joiner"), nil)
	page.leaf = true

	page = entry({"admin", "network", "thread_handler_setting"}, post("thread_handler_setting"), nil)
	page.leaf = true

	page = entry({"admin", "network", "thread_stop"}, post("thread_stop"), nil)
	page.leaf = true
end

function thread_handler_setting()
	local ubus = require "ubus"
	local tpl = require "luci.template"
	local http = require "luci.http"
	local networkname = luci.http.formvalue("threadname")
	local channel = luci.http.formvalue("channel") + 0
	local panid = luci.http.formvalue("panid")
	local extpanid = luci.http.formvalue("extpanid")
	local mode = luci.http.formvalue("mode")
	local leaderpartitionid = luci.http.formvalue("leaderpartitionid") + 0
	local masterkey = luci.http.formvalue("masterkey")
	local pskc = luci.http.formvalue("pskc")
	local macfilter = luci.http.formvalue("macfilterselect")
	local macfilteradd = luci.http.formvalue("macfilteradd")
	local macfilterremove = luci.http.formvalue("macfilterremove")
	local submitcontent = luci.http.formvalue("submitcontent")
	local jumpflag = 0

	local conn = ubus.connect()

	if not conn then
		error("Failed to connect to ubusd")
	end

	if submitcontent == "enable" then
		conn:call("otbr", "threadstart", {})
	elseif submitcontent == "disable" then
		conn:call("otbr", "threadstop", {})
	elseif submitcontent == "leave" then
		conn:call("otbr", "leave", {})
	elseif submitcontent == "addAddr" then
		conn:call("otbr", "macfilteradd", { addr = macfilteradd })
		jumpflag = 1
	elseif submitcontent == "removeAddr" then
		local removeAddrIndex = luci.http.formvalue("removeAddrIndex") + 0
		conn:call("otbr", "macfilterremove", { addr = macfilterremove[removeAddrIndex] })
		jumpflag = 1
	else
		if(threadget("state") == "disabled")then
			conn:call("otbr", "setnetworkname", { networkname = networkname })
			conn:call("otbr", "setchannel", { channel = channel })
			conn:call("otbr", "setpanid", { panid = panid })
			conn:call("otbr", "setextpanid", { extpanid = extpanid })
			conn:call("otbr", "setmode", { mode = mode })
			conn:call("otbr", "setleaderpartitionid", { leaderpartitionid = leaderpartitionid })
			conn:call("otbr", "setmasterkey", { masterkey = masterkey })
			conn:call("otbr", "setpskc", { pskc = pskc })
			conn:call("otbr", "macfiltersetstate", { state = macfilter })
			conn:call("otbr", "threadstart", {})
		else
			conn:call("otbr", "mgmtset", { masterkey = masterkey, networkname = networkname, extpanid = extpanid, panid = panid, channel = tostring(channel), pskc = pskc })
			conn:call("otbr", "macfiltersetstate", { state = macfilter })
		end
	end

	if(jumpflag == 0) then
		local stat, dsp = pcall(require, "luci.dispatcher")
		luci.http.redirect(stat and dsp.build_url("admin", "network", "thread"))
	else
		local stat, dsp = pcall(require, "luci.dispatcher")
		luci.http.redirect(stat and dsp.build_url("admin", "network", "thread_setting"))
	end
end

function thread_add()
	local ubus = require "ubus"
	local tpl = require "luci.template"
	local http = require "luci.http"

	local conn = ubus.connect()

	if not conn then
		error("Failed to connect to ubusd")
	end

	conn:call("otbr", "commissionerstart", {})

	local stat, dsp = pcall(require, "luci.dispatcher")
	luci.http.redirect(stat and dsp.build_url("admin", "network", "thread_add_page"))
end

function thread_add_joiner()
	local ubus = require "ubus"
	local tpl = require "luci.template"
	local http = require "luci.http"
	local pskd = luci.http.formvalue("pskd")
	local eui64 = luci.http.formvalue("eui64")

	local conn = ubus.connect()

	if not conn then
		error("Failed to connect to ubusd")
	end

	conn:call("otbr", "joineradd", { pskd = pskd, eui64 = eui64})

	local stat, dsp = pcall(require, "luci.dispatcher")
	luci.http.redirect(stat and dsp.build_url("admin", "network", "thread_view"))
end

function joiner_remove()
	local ubus = require "ubus"

	local conn = ubus.connect()

	if not conn then
		error("Failed to connect to ubusd")
	end

	conn:call("otbr", "joinerremove", {})

	local stat, dsp = pcall(require, "luci.dispatcher")
	luci.http.redirect(stat and dsp.build_url("admin", "network", "thread_view"))
end

function thread_attach()
	local ubus = require "ubus"
	local tpl = require "luci.template"
	local http = require "luci.http"
	local panid = luci.http.formvalue("panid")
	local channel = luci.http.formvalue("channel") + 0
	local masterkey = luci.http.formvalue("masterkey")

	local conn = ubus.connect()

	if not conn then
		error("Failed to connect to ubusd")
	end

	conn:call("otbr", "setpanid", { panid = panid })
	conn:call("otbr", "setchannel", { channel = channel })
	conn:call("otbr", "setmasterkey", { masterkey = masterkey })
	conn:call("otbr", "threadstart", {})

	local stat, dsp = pcall(require, "luci.dispatcher")
	luci.http.redirect(stat and dsp.build_url("admin", "network", "thread"))
end

function thread_stop()
	local ubus = require "ubus"
	local tpl = require "luci.template"
	local http = require "luci.http"

	local conn = ubus.connect()

	if not conn then
		error("Failed to connect to ubusd")
	end

	conn:call("otbr", "threadstop", {})

	local stat, dsp = pcall(require, "luci.dispatcher")
	luci.http.redirect(stat and dsp.build_url("admin", "network", "thread"))
end

function thread_join()
	local tpl = require "luci.template"
	local http = require "luci.http"

	local stat, dsp = pcall(require, "luci.dispatcher")
	luci.http.redirect(stat and dsp.build_url("admin", "network", "thread_join"))
end

function thread_state()
	luci.http.prepare_content("application/json")

	local result = {}
	result.state = threadget("state")

	if(result.status ~= "disabled") then
		result.panid = threadget("panid")
		result.channel = threadget("channel")
		result.networkname = threadget("networkname")
	end
	luci.http.write_json(result)
end

function thread_graph()
	luci.http.prepare_content("application/json")

	luci.http.write_json(networkdata())
end

function thread_neighbors()
	luci.http.prepare_content("application/json")

	local result = {}

	result.neighbor = neighborlist()
	result.joinernum = threadget("joinernum")
	result.state = threadget("state")

	luci.http.write_json(result)
end

function networkdata()
	local k, v, m, n
	local data = { }
	local l = { }

	local result = connect_ubus("networkdata")

	for k, v in pairs(result) do
		l[#l+1] = v
	end

	data.connect = l
	data.state = threadget("state")
	data.rloc16 = threadget("rloc16")
	data.joinernum = threadget("joinernum")
	data.leader = threadget("leaderdata").LeaderRouterId
	return data
end

function neighborlist()
	local k, v, m, n, result
	local l = { }

	local state = threadget("state")

	if state == 'child' then
		result = connect_ubus("parent")
		for k, v in pairs(result) do
			l[#l+1] = v
		end
	else
		result = connect_ubus("neighbor")
		for k, v in pairs(result) do
			for m, n in pairs(v) do
				-- n is the table of neighborlist item
				l[#l+1] = n
			end
		end
	end

	return l
end

function connect_ubus(methods)
	local ubus = require "ubus"
	local result
	local conn = ubus.connect()

	if not conn then
		error("Failed to connect to ubusd")
	end

	result = conn:call("otbr", methods, {})

	return result
end

function threadget(action)
	local getresult = connect_ubus(action)
	local k, v, result

	for k, v in pairs(getresult) do
		result = v
	end

	return result
end
