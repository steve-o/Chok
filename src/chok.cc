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

using rfa::common::RFA_String;

static std::weak_ptr<rfa::common::EventQueue> g_event_queue;

chok::chok_t::chok_t()
{
}

chok::chok_t::~chok_t()
{
	clear();
	LOG(INFO) << "fin.";
}

int
chok::chok_t::run ()
{
	LOG(INFO) << config_;

	try {
/* RFA context. */
		rfa_.reset (new rfa_t (config_));
		if (!(bool)rfa_ || !rfa_->init())
			goto cleanup;

/* RFA asynchronous event queue. */
		const RFA_String eventQueueName (config_.event_queue_name.c_str(), 0, false);
		event_queue_.reset (rfa::common::EventQueue::create (eventQueueName), std::mem_fun (&rfa::common::EventQueue::destroy));
		if (!(bool)event_queue_)
			goto cleanup;
/* Create weak pointer to handle application shutdown. */
		g_event_queue = event_queue_;

/* RFA logging. */
		log_.reset (new logging::LogEventProvider (config_, event_queue_));
		if (!(bool)log_ || !log_->Register())
			goto cleanup;

#if 0
/* RFA consumer. */
		consumer_.reset (new consumer_t (config_, rfa_, event_queue_));
		if (!(bool)consumer_ || !consumer_->init())
			goto cleanup;

/* Create state for subscribed RIC. */
		static const std::string msft ("MSFT.O");
		auto stream = std::make_shared<subscription_stream_t> ();
		if (!(bool)stream)
			goto cleanup;
		if (!consumer_->createItemStream (msft.c_str(), stream))
			goto cleanup;
		msft_stream_ = std::move (stream);
#endif

	} catch (rfa::common::InvalidUsageException& e) {
		LOG(ERROR) << "InvalidUsageException: { "
			"Severity: \"" << severity_string (e.getSeverity()) << "\""
			", Classification: \"" << classification_string (e.getClassification()) << "\""
			", StatusText: \"" << e.getStatus().getStatusText() << "\" }";
		goto cleanup;
	} catch (rfa::common::InvalidConfigurationException& e) {
		LOG(ERROR) << "InvalidConfigurationException: { "
			"Severity: \"" << severity_string (e.getSeverity()) << "\""
			", Classification: \"" << classification_string (e.getClassification()) << "\""
			", StatusText: \"" << e.getStatus().getStatusText() << "\""
			", ParameterName: \"" << e.getParameterName() << "\""
			", ParameterValue: \"" << e.getParameterValue() << "\" }";
		goto cleanup;
	}

	LOG(INFO) << "Init complete, entering main loop.";
	mainLoop ();

	LOG(INFO) << "Main loop terminated.";
	return EXIT_SUCCESS;
cleanup:
	LOG(INFO) << "Init failed, cleaning up.";
	clear();
	return EXIT_FAILURE;
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
chok::chok_t::mainLoop()
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
chok::chok_t::clear()
{
/* Signal message pump thread to exit. */
	if ((bool)event_queue_)
		event_queue_->deactivate();

	msft_stream_.reset();

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
