/* User-configurable settings.
 *
 * NB: all strings are locale bound, RFA provides no Unicode support.
 */

#ifndef __CONFIG_HH__
#define __CONFIG_HH__
#pragma once

#include <string>
#include <sstream>
#include <vector>

namespace chok
{

	struct config_t
	{
		config_t();

//  SNMP implant.
		bool is_snmp_enabled;

//  Net-SNMP agent or sub-agent.
		bool is_agentx_subagent;

//  Net-SNMP file log target.
		std::string snmp_filelog;

//  AgentX port number to connect to master agent.
		std::string agentx_socket;

//  Windows registry key path.
		std::string key;

//  TREP-RT service name, e.g. IDN_RDF.
		std::string service_name;

//  TREP-RT ADH hostname or IP address.
		std::vector<std::string> rssl_servers;

//  Default TREP-RT RSSL port, e.g. 14002, 14003.
		std::string rssl_default_port;

/* DACS application Id.  If the server authenticates with DACS, the consumer
 * application may be required to pass in a valid ApplicationId.
 * Range: "" (None) or 1-511 as an Ascii string.
 */
		std::string application_id;

/* InstanceId is used to differentiate applications running on the same host.
 * If there is more than one noninteractive provider instance running on the
 * same host, they must be set as a different value by the provider
 * application. Otherwise, the infrastructure component which the providers
 * connect to will reject a login request that has the same InstanceId value
 * and cut the connection.
 * Range: "" (None) or any Ascii string, presumably to maximum RFA_String length.
 */
		std::string instance_id;

/* DACS username, frequently non-checked and set to similar: user1.
 */
		std::string user_name;

/* DACS position, the station which the user is using.
 * Range: "" (None) or "<IPv4 address>/hostname" or "<IPv4 address>/net"
 */
		std::string position;

//  RFA session name.
		std::string session_name;

//  RFA application logger monitor name.
		std::string monitor_name;

//  RFA event queue name.
		std::string event_queue_name;

//  RFA connection name.
		std::string connection_name;

//  RFA consumer name.
		std::string consumer_name;

//  Subscription list.
		std::vector<std::string> instruments;
	};

	inline
	std::ostream& operator<< (std::ostream& o, const config_t& config) {
		std::ostringstream rssl_servers, instruments;
		for (auto it = config.rssl_servers.begin(); it != config.rssl_servers.end(); ++it) {
			if (it != config.rssl_servers.begin())
				rssl_servers << ", ";
			rssl_servers << '"' << *it << '"';
		}		
		for (auto it = config.instruments.begin(); it != config.instruments.end(); ++it) {
			if (it != config.instruments.begin())
				instruments << ", ";
			instruments << '"' << *it << '"';
		}		
		o << "\"config_t\": { "
			  "\"service_name\": \"" << config.service_name << "\""
			", \"rssl_servers\": [" << rssl_servers.str() << "]"
			", \"rssl_default_port\": \"" << config.rssl_default_port << "\""
			", \"application_id\": \"" << config.application_id << "\""
			", \"instance_id\": \"" << config.instance_id << "\""
			", \"user_name\": \"" << config.user_name << "\""
			", \"position\": \"" << config.position << "\" }"
			", \"session_name\": \"" << config.session_name << "\""
			", \"monitor_name\": \"" << config.monitor_name << "\""
			", \"event_queue_name\": \"" << config.event_queue_name << "\""
			", \"connection_name\": \"" << config.connection_name << "\""
			", \"consumer_name\": \"" << config.consumer_name << "\""
			", \"instruments\": [" << instruments.str() << "]"
			" }";
		return o;
	}

} /* namespace chok */

#endif /* __CONFIG_HH__ */

/* eof */
