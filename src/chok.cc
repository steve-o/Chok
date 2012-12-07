/* RDM subscriber application.
 */

#include "chok.hh"

#define __STDC_FORMAT_MACROS
#include <cstdint>
#include <inttypes.h>

#include <winsock2.h>

#include "chromium/logging.hh"
#include "error.hh"
#include "rfa_logging.hh"
#include "rfaostream.hh"
#include "snmp_agent.hh"

/* RDM Usage Guide: Section 6.5: Enterprise Platform
 * For future compatibility, the DictionaryId should be set to 1 by providers.
 * The DictionaryId for the RDMFieldDictionary is 1.
 */
static const int kDictionaryId = 1;

/* RDM: Absolutely no idea. */
static const int kFieldListId = 3;

/* RDM Field Identifiers. */
static const int kRdmRdnDisplayId = 2;		/* RDNDISPLAY */
static const int kRdmTradePriceId = 6;		/* TRDPRC_1 */

std::list<chok::chok_t*> chok::chok_t::global_list_;
boost::shared_mutex chok::chok_t::global_list_lock_;

static std::weak_ptr<rfa::common::EventQueue> g_event_queue;


using rfa::common::RFA_String;

chok::chok_t::chok_t()
{
	boost::unique_lock<boost::shared_mutex> (global_list_lock_);
	global_list_.push_back (this);
}

chok::chok_t::~chok_t()
{
/* Remove from list before clearing. */
	boost::unique_lock<boost::shared_mutex> (global_list_lock_);
	global_list_.remove (this);

	Clear();
	LOG(INFO) << "fin.";
}

bool
chok::chok_t::Init ()
{
	LOG(INFO) << config_;

	try {
/* RFA context. */
		rfa_.reset (new rfa_t (config_));
		if (!(bool)rfa_ || !rfa_->Init())
			return false;

/* RFA asynchronous event queue. */
		const RFA_String eventQueueName (config_.event_queue_name.c_str(), 0, false);
		event_queue_.reset (rfa::common::EventQueue::create (eventQueueName), std::mem_fun (&rfa::common::EventQueue::destroy));
		if (!(bool)event_queue_)
			return false;
/* Create weak pointer to handle application shutdown. */
		g_event_queue = event_queue_;

/* RFA logging. */
		log_.reset (new logging::rfa::LogEventProvider (config_, event_queue_));
		if (!(bool)log_ || !log_->Register())
			return false;
/* RFA consumer. */
		consumer_.reset (new consumer_t (config_, rfa_, event_queue_));
		if (!(bool)consumer_ || !consumer_->Init())
			return false;

/* Create state for subscribed RIC. */
		for (auto it = config_.instruments.begin(); it != config_.instruments.end(); ++it) {
			auto stream = std::make_shared<subscription_stream_t> ();
			if (!(bool)stream)
				return false;
			if (!consumer_->CreateItemStream (it->c_str(), stream))
				return false;
			streams_.push_back (stream);
			DLOG(INFO) << *it;
		}

	} catch (const rfa::common::InvalidUsageException& e) {
		LOG(ERROR) << "InvalidUsageException: { "
			  "\"Severity\": \"" << internal::severity_string (e.getSeverity()) << "\""
			", \"Classification\": \"" << internal::classification_string (e.getClassification()) << "\""
			", \"StatusText\": \"" << e.getStatus().getStatusText() << "\" }";
		return false;
	} catch (const rfa::common::InvalidConfigurationException& e) {
		LOG(ERROR) << "InvalidConfigurationException: { "
			  "\"Severity\": \"" << internal::severity_string (e.getSeverity()) << "\""
			", \"Classification\": \"" << internal::classification_string (e.getClassification()) << "\""
			", \"StatusText\": \"" << e.getStatus().getStatusText() << "\""
			", \"ParameterName\": \"" << e.getParameterName() << "\""
			", \"ParameterValue\": \"" << e.getParameterValue() << "\" }";
		return false;
	} catch (const std::exception& e) {
		LOG(ERROR) << "Rfa::Exception: { "
			"\"What\": \"" << e.what() << "\" }";
		return false;
	}

	try {
/* Spawn SNMP implant. */
		if (config_.is_snmp_enabled) {
			snmp_agent_.reset (new snmp_agent_t (*this));
			if (!(bool)snmp_agent_)
				return false;
		}
	} catch (const std::exception& e) {
		LOG(ERROR) << "SnmpAgent::Exception: { "
			"\"What\": \"" << e.what() << "\" }";
		return false;
	}
	return true;
}

int
chok::chok_t::Run()
{
	if (!Init()) {
		LOG(INFO) << "Init failed, cleaning up.";
		Clear();
		return EXIT_FAILURE;
	}

	LOG(INFO) << "Init complete, entering main loop.";
	MainLoop ();
	LOG(INFO) << "Main loop terminated.";
	Clear();
	return EXIT_SUCCESS;
}

/* On a shutdown event set a global flag and force the event queue
 * to catch the event by submitting a log event.
 */
static
BOOL
CtrlHandler (
	DWORD	fdwCtrlType
	)
{
	const char* message;
	switch (fdwCtrlType) {
	case CTRL_C_EVENT:
		message = "Caught ctrl-c event, shutting down";
		break;
	case CTRL_CLOSE_EVENT:
		message = "Caught close event, shutting down";
		break;
	case CTRL_BREAK_EVENT:
		message = "Caught ctrl-break event, shutting down";
		break;
	case CTRL_LOGOFF_EVENT:
		message = "Caught logoff event, shutting down";
		break;
	case CTRL_SHUTDOWN_EVENT:
	default:
		message = "Caught shutdown event, shutting down";
		break;
	}
/* if available, deactivate global event queue pointer to break running loop. */
	if (!g_event_queue.expired()) {
		auto sp = g_event_queue.lock();
		sp->deactivate();
	}
	LOG(INFO) << message;
	return TRUE;
}

void
chok::chok_t::MainLoop()
{
/* Add shutdown handler. */
	::SetConsoleCtrlHandler ((PHANDLER_ROUTINE)::CtrlHandler, TRUE);
	while (event_queue_->isActive()) {
		event_queue_->dispatch (rfa::common::Dispatchable::InfiniteWait);
	}
/* Remove shutdown handler. */
	::SetConsoleCtrlHandler ((PHANDLER_ROUTINE)::CtrlHandler, FALSE);
}

void
chok::chok_t::Clear()
{
/* Signal message pump thread to exit. */
	if ((bool)event_queue_)
		event_queue_->deactivate();

/* Purge subscription streams. */
	streams_.clear();

/* Release everything with an RFA dependency. */
	assert (consumer_.use_count() <= 1);
	consumer_.reset();
	assert (log_.use_count() <= 1);
	log_.reset();
	assert (event_queue_.use_count() <= 1);
	event_queue_.reset();
	assert (rfa_.use_count() <= 1);
	rfa_.reset();
}

/* eof */
