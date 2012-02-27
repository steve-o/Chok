/* User-configurable settings.
 */

#include "config.hh"

static const char* kDefaultAdsPort = "14002";

chok::config_t::config_t() :
/* default values */
	service_name ("IDN_RDF"),
	rssl_default_port (kDefaultAdsPort),
	application_id ("256"),
	instance_id ("Instance1"),
	user_name ("user1"),
	position ("127.0.0.1/net"),
	session_name ("SessionName"),
	monitor_name ("ApplicationLoggerMonitorName"),
	event_queue_name ("EventQueueName"),
	connection_name ("ConnectionName"),
	consumer_name ("ConsumerName")
{
/* C++11 initializer lists not supported in MSVC2010 */
	rssl_servers.push_back ("nylabads2");
}

/* eof */
