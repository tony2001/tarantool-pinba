-- pinba.client

local lib = require('pinba.lib') -- native library

local function test(test_param)
	lib.test()
    return ""
end

local function errorf(fmt, ...)
	error(string.format(fmt, ...))
end

local function sprintf(fmt, ...)
	return string.format(fmt, ...)
end

-- methods 

local function set_hostname(self, value)
	if type(value) ~= 'string' then
		errorf("hostname must be a string, %s given", type(value))
	end
	self.data.hostname = value
end

local function set_scriptname(self, value)
	if type(value) ~= 'string' then
		errorf("scriptname must be a string, %s given", type(value))
	end
	self.data.script_name = value 
end

local function set_servername(self, value)
	if type(value) ~= 'string' then
		errorf("servername must be a string, %s given", type(value))
	end
	self.data.server_name = value
end

local function set_requestcount(self, value)
	if type(value) ~= 'number' then
		errorf("requestcount must be a number, %s given", type(value))
	end
	self.data.request_count = value
end

local function set_documentsize(self, value)
	if type(value) ~= 'number' then
		errorf("documentsize must be a number, %s given", type(value))
	end
	self.data.document_size = value 
end

local function set_memorypeak(self, value)
	if type(value) ~= 'number' then
		errorf("memorypeak must be a number, %s given", type(value))
	end
	self.data.memory_peak = value
end

local function set_memoryfootprint(self, value)
	if type(value) ~= 'number' then
		errorf("memoryfootprint must be a number, %s given", type(value))
	end
	self.data.memory_footprint = value
end

local function set_rusage(self, value)
	if type(value) ~= 'table' then
		errorf("rusage must be a table, %s given", type(value))
	end

	if table.getn(value) ~= 2 then 
		errorf("rusage must be a table and have exactly 2 items, not a %d items", table.getn(value))
	end

	self.data.ru_utime = value[1]
	self.data.ru_stime = value[2]
end

local function set_requesttime(self, value)
	if type(value) ~= 'number' then
		errorf("requesttime must be a number, %s given", type(value))
	end
	self.data.request_time = value
end

local function set_status(self, value)
	if type(value) ~= 'number' then
		errorf("status must be a number, %s given", type(value))
	end
	self.data.status = value
end

local function set_schema(self, value)
	if type(value) ~= 'string' then
		errorf("schema must be a string, %s given", type(value))
	end
	self.data.schema = value 
end

local function set_tag(self, tag, value)
	if type(tag) ~= 'string' then
		errorf("tag must be a string, %s given", type(tag))
	end
	if type(value) ~= 'string' then
		errorf("value must be a string, %s given", type(value))
	end

	self.data.tags[tag] = value 
end

local function send(self)
	lib.send(self.host, self.port, self.data)
end

-- constructor

local function construct(host, port)
	if type(host) ~= 'string' then
		errorf("host must be a string, %s given", type(host))
	end

	if type(port) ~= 'number' then
		errorf("port must be a number, %s given", type(port))
	end

	if port <= 0 then
		errorf("port must be greater than 0, %d given", port)
	end

	local self = {
		host = host,
		port = port,
		data = {
			hostname = "",
			server_name = "",
			script_name = "",
			request_count = 1,
			document_size = 0,
			memory_peak = 0,
			memory_footprint = 0,
			request_time = 0,
			ru_utime = 0,
			ru_stime = 0,
			status = 0,
			schema = "",
			timers = {},
			tags = {}
		},

		-- methods
		set_hostname = set_hostname,
		set_scriptname = set_scriptname,
		set_servername = set_servername,
		set_requestcount = set_requestcount,
		set_documentsize = set_documentsize,
		set_memorypeak = set_memorypeak,
		set_memoryfootprint = set_memoryfootprint,
		set_rusage = set_rusage,
		set_requesttime = set_requesttime,
		set_status = set_status,
		set_schema = set_schema,
		set_tag = set_tag,
		send = send
	}
	return self
end

return {
	new = construct,
	test = test
}
