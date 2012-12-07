/* RFA consumer.
 *
 * One single consumer, and hence wraps a RFA session for simplicity.
 * Connection events (7.4.7.4, 7.5.8.3) are ignored as they're completely
 * useless.
 *
 * Definition of overlapping terms:
 *   OMM Consumer:  Underlying RFA consumer object.
 *   Consumer:      Application encapsulation of consumer functionality.
 *   Session:       RFA session object that contains one or more "Connection"
 *                  objects for horizontal scaling, e.g. RDF, GARBAN, TOPIC3.
 *   Connection:    RFA connection object that contains one or more servers.
 *   Server List:   A list of servers with round-robin failover connectivity.
 */

#include "consumer.hh"

#include <algorithm>
#include <utility>

#include <winsock2.h>

#include "chromium/logging.hh"
#include "error.hh"
#include "rfaostream.hh"

using rfa::common::RFA_String;

/* Reuters Wire Format nomenclature for dictionary names. */
static const RFA_String kRdmFieldDictionaryName ("RWFFld");
static const RFA_String kEnumTypeDictionaryName ("RWFEnum");

chok::consumer_t::consumer_t (
	const chok::config_t& config,
	std::shared_ptr<chok::rfa_t> rfa,
	std::shared_ptr<rfa::common::EventQueue> event_queue
	) :
	last_activity_ (boost::posix_time::microsec_clock::universal_time()),
	config_ (config),
	rfa_ (rfa),
	event_queue_ (event_queue),
	error_item_handle_ (nullptr),
	item_handle_ (nullptr),
	rwf_major_version_ (0),
	rwf_minor_version_ (0),
	is_muted_ (true)
{
	ZeroMemory (cumulative_stats_, sizeof (cumulative_stats_));
	ZeroMemory (snap_stats_, sizeof (snap_stats_));
}

chok::consumer_t::~consumer_t()
{
	VLOG(3) << "Unregistering RFA session clients.";
	if (nullptr != item_handle_)
		omm_consumer_->unregisterClient (item_handle_), item_handle_ = nullptr;
	if (nullptr != error_item_handle_)
		omm_consumer_->unregisterClient (error_item_handle_), error_item_handle_ = nullptr;
	omm_consumer_.reset();
	session_.reset();
}

bool
chok::consumer_t::Init()
{
	last_activity_ = boost::posix_time::microsec_clock::universal_time();

/* 7.2.1 Configuring the Session Layer Package.
 */
	VLOG(3) << "Acquiring RFA session.";
	const RFA_String sessionName (config_.session_name.c_str(), 0, false);
	session_.reset (rfa::sessionLayer::Session::acquire (sessionName));
	if (!(bool)session_)
		return false;

/* 6.2.2.1 RFA Version Info.  The version is only available if an application
 * has acquired a Session (i.e., the Session Layer library is loaded).
 */
	LOG(INFO) << "RFA: { \"productVersion\": \"" << rfa::common::Context::getRFAVersionInfo()->getProductVersion() << "\" }";

/* 7.5.6 Initializing an OMM Non-Interactive Provider. */
	VLOG(3) << "Creating OMM consumer.";
	const RFA_String consumerName (config_.consumer_name.c_str(), 0, false);
	omm_consumer_.reset (session_->createOMMConsumer (consumerName, nullptr));
	if (!(bool)omm_consumer_)
		return false;

/* 7.5.7 Registering for Events from an OMM Non-Interactive Provider. */
/* receive error events (OMMCmdErrorEvent) related to calls to submit(). */
	VLOG(3) << "Registering OMM error interest.";	
	rfa::sessionLayer::OMMErrorIntSpec ommErrorIntSpec;
	error_item_handle_ = omm_consumer_->registerClient (event_queue_.get(), &ommErrorIntSpec, *this, nullptr /* closure */);
	if (nullptr == error_item_handle_)
		return false;

	return SendLoginRequest();
}

/* 7.3.5.3 Making a Login Request	
 * A Login request message is encoded and sent by OMM Consumer and OMM non-
 * interactive provider applications.
 */
bool
chok::consumer_t::SendLoginRequest()
{
	VLOG(2) << "Sending login request.";
	rfa::message::ReqMsg request;
	request.setMsgModelType (rfa::rdm::MMT_LOGIN);
	request.setInteractionType (rfa::message::ReqMsg::InitialImageFlag | rfa::message::ReqMsg::InterestAfterRefreshFlag);

	rfa::message::AttribInfo attribInfo;
	attribInfo.setNameType (rfa::rdm::USER_NAME);
	const RFA_String userName (config_.user_name.c_str(), 0, false);
	attribInfo.setName (userName);

/* The request attributes ApplicationID and Position are encoded as an
 * ElementList (5.3.4).
 */
	rfa::data::ElementList elementList;
	rfa::data::ElementListWriteIterator it;
	it.start (elementList);

/* DACS Application Id.
 * e.g. "256"
 */
	rfa::data::ElementEntry element;
	element.setName (rfa::rdm::ENAME_APP_ID);
	rfa::data::DataBuffer elementData;
	const RFA_String applicationId (config_.application_id.c_str(), 0, false);
	elementData.setFromString (applicationId, rfa::data::DataBuffer::StringAsciiEnum);
	element.setData (elementData);
	it.bind (element);

/* DACS Position name.
 * e.g. "localhost"
 */
	element.setName (rfa::rdm::ENAME_POSITION);
	const RFA_String position (config_.position.c_str(), 0, false);
	elementData.setFromString (position, rfa::data::DataBuffer::StringAsciiEnum);
	element.setData (elementData);
	it.bind (element);

/* Instance Id (optional).
 * e.g. "<Instance Id>"
 */
	if (!config_.instance_id.empty())
	{
		element.setName (rfa::rdm::ENAME_INST_ID);
		const RFA_String instanceId (config_.instance_id.c_str(), 0, false);
		elementData.setFromString (instanceId, rfa::data::DataBuffer::StringAsciiEnum);
		element.setData (elementData);
		it.bind (element);
	}

	it.complete();
	attribInfo.setAttrib (elementList);
	request.setAttribInfo (attribInfo);

/* 4.2.8 Message Validation.  RFA provides an interface to verify that
 * constructed messages of these types conform to the Reuters Domain
 * Models as specified in RFA API 7 RDM Usage Guide.
 */
	uint8_t validation_status = rfa::message::MsgValidationError;
	try {
		RFA_String warningText;
		validation_status = request.validateMsg (&warningText);
		if (rfa::message::MsgValidationWarning == validation_status)
			LOG(WARNING) << "MMT_LOGIN::validateMsg: { \"warningText\": \"" << warningText << "\" }";
		cumulative_stats_[CONSUMER_PC_MMT_LOGIN_VALIDATED]++;
	} catch (const rfa::common::InvalidUsageException& e) {
		LOG(ERROR) << "InvalidUsageException: { " <<
				   "\"StatusText\": \"" << e.getStatus().getStatusText() << "\""
				", " << request <<
			      " }";
		cumulative_stats_[CONSUMER_PC_MMT_LOGIN_MALFORMED]++;
	} catch (const std::exception& e) {
		LOG(ERROR) << "Rfa::Exception: { "
			  "\"What\": \"" << e.what() << "\""
			", " << request <<
			" }";
		cumulative_stats_[CONSUMER_PC_MMT_LOGIN_EXCEPTION]++;
	}

/* Not saving the returned handle as we will destroy the /consumer/ to logout,
 * reference:
 * 7.4.10.6 Other Cleanup
 * Note: The application may call destroy() on an Event Source without having
 * closed all Event Streams. RFA will internally unregister all open Event
 * Streams in this case.
 */
	VLOG(3) << "Registering OMM item interest for MMT_LOGIN.";
	rfa::sessionLayer::OMMItemIntSpec ommItemIntSpec;
	ommItemIntSpec.setMsg (&request);
	item_handle_ = omm_consumer_->registerClient (event_queue_.get(), &ommItemIntSpec, *this, nullptr /* closure */);
	cumulative_stats_[CONSUMER_PC_MMT_LOGIN_SENT]++;
	if (nullptr == item_handle_)
		return false;

/* Store negotiated Reuters Wire Format version information. */
	rfa::data::Map map;
	map.setAssociatedMetaInfo (*item_handle_);
	rwf_major_version_ = map.getMajorVersion();
	rwf_minor_version_ = map.getMinorVersion();
	LOG(INFO) << "RWF: { "
		     "\"MajorVersion\": " << (unsigned)rwf_major_version_ <<
		   ", \"MinorVersion\": " << (unsigned)rwf_minor_version_ <<
		   " }";
	return true;
}

bool
chok::consumer_t::SendItemRequest (
	std::shared_ptr<item_stream_t> item_stream	
	)
{
	VLOG(2) << "Sending market price request.";
	rfa::message::ReqMsg request;

	request.setMsgModelType (rfa::rdm::MMT_MARKET_PRICE);
/* we don't care about the initial image but the API will complain if not requested.
 *
 * InvalidUsageException: {
 *   "StatusText": "InteractionType without expected 'ReqMsg::InitialImageFlag' is Invalid."
 * }
 */
	request.setInteractionType (rfa::message::ReqMsg::InitialImageFlag | rfa::message::ReqMsg::InterestAfterRefreshFlag);

	rfa::message::AttribInfo attribInfo;
	attribInfo.setNameType (rfa::rdm::INSTRUMENT_NAME_RIC);
	attribInfo.setName (item_stream->rfa_item_name);
	attribInfo.setServiceName (item_stream->rfa_service_name);

	request.setAttribInfo (attribInfo);

	if (DCHECK_IS_ON()) {
/* 4.2.8 Message Validation.  RFA provides an interface to verify that
 * constructed messages of these types conform to the Reuters Domain
 * Models as specified in RFA API 7 RDM Usage Guide.
 */
		uint8_t validation_status = rfa::message::MsgValidationError;
		try {
			RFA_String warningText;
			validation_status = request.validateMsg (&warningText);
			if (rfa::message::MsgValidationWarning == validation_status)
				LOG(WARNING) << "validateMsg: { \"warningText\": \"" << warningText << "\" }";
			cumulative_stats_[CONSUMER_PC_MMT_MARKET_PRICE_REQUEST_VALIDATED]++;
		} catch (const rfa::common::InvalidUsageException& e) {
			LOG(ERROR) << "InvalidUsageException: { " <<
					   "\"StatusText\": \"" << e.getStatus().getStatusText() << "\""
					", " << request <<
				      " }";
			cumulative_stats_[CONSUMER_PC_MMT_MARKET_PRICE_REQUEST_MALFORMED]++;
		} catch (const std::exception& e) {
			LOG(ERROR) << "Rfa::Exception: { "
				  "\"What\": \"" << e.what() << "\""
				", " << request <<
				" }";
			cumulative_stats_[CONSUMER_PC_MMT_MARKET_PRICE_REQUEST_EXCEPTION]++;
		}
	}

	VLOG(3) << "Registering OMM item interest for MMT_MARKET_PRICE.";
	rfa::sessionLayer::OMMItemIntSpec ommItemIntSpec;
	ommItemIntSpec.setMsg (&request);
	item_stream->item_handle = omm_consumer_->registerClient (event_queue_.get(), &ommItemIntSpec, *this, item_stream.get() /* closure */);
	cumulative_stats_[CONSUMER_PC_MMT_MARKET_PRICE_REQUEST_SENT]++;
	if (nullptr == item_stream->item_handle)
		return false;
	return true;
}

/* Re-register entire directory for new handles.
 */
bool
chok::consumer_t::Resubscribe ()
{
	if (!(bool)omm_consumer_) {
		LOG(WARNING) << "Resubscribe whilst consumer is invalid.";
		return false;
	}

/* Cannot use std::for_each (auto λ) due to language limitations. */
	std::for_each (directory_.begin(), directory_.end(), [&](std::pair<std::string, std::weak_ptr<item_stream_t>> it) {
		if (auto sp = it.second.lock()) {
/* only non-fulfilled items */
			if (nullptr == sp->item_handle)
				SendItemRequest (sp);
		}
	});
	return true;
}

/* Create an item stream for a given symbol name.  The Item Stream maintains
 * the provider state on behalf of the application.
 */
bool
chok::consumer_t::CreateItemStream (
	const char* item_name,
	std::shared_ptr<item_stream_t> item_stream
	)
{
	VLOG(4) << "Creating item stream for RIC \"" << item_name << "\".";
	item_stream->rfa_item_name.set (item_name, 0, true);
	item_stream->rfa_service_name.set (config_.service_name.c_str(), 0, true);
	if (!is_muted_) {
		if (!SendItemRequest (item_stream))
			return false;
	} else {
/* no-op */
	}
	const std::string key (item_name);
	auto status = directory_.emplace (std::make_pair (key, item_stream));
	assert (true == status.second);
	assert (directory_.end() != directory_.find (key));
	DVLOG(4) << "Directory size: " << directory_.size();
	last_activity_ = boost::posix_time::microsec_clock::universal_time();
	return true;
}

void
chok::consumer_t::processEvent (
	const rfa::common::Event& event_
	)
{
	VLOG(1) << event_;
	cumulative_stats_[CONSUMER_PC_RFA_EVENTS_RECEIVED]++;
	switch (event_.getType()) {
	case rfa::sessionLayer::OMMItemEventEnum:
		OnOMMItemEvent (static_cast<const rfa::sessionLayer::OMMItemEvent&>(event_));
		break;

        case rfa::sessionLayer::OMMCmdErrorEventEnum:
                OnOMMCmdErrorEvent (static_cast<const rfa::sessionLayer::OMMCmdErrorEvent&>(event_));
                break;

        default:
		cumulative_stats_[CONSUMER_PC_RFA_EVENTS_DISCARDED]++;
		LOG(WARNING) << "Uncaught: " << event_;
                break;
        }
}

/* 7.5.8.1 Handling Item Events (Login Events).
 */
void
chok::consumer_t::OnOMMItemEvent (
	const rfa::sessionLayer::OMMItemEvent&	item_event
	)
{
	cumulative_stats_[CONSUMER_PC_OMM_ITEM_EVENTS_RECEIVED]++;
	const rfa::common::Msg& msg = item_event.getMsg();

/* Verify event is a response event */
	if (rfa::message::RespMsgEnum != msg.getMsgType()) {
		cumulative_stats_[CONSUMER_PC_OMM_ITEM_EVENTS_DISCARDED]++;
		LOG(WARNING) << "Uncaught: " << msg;
		return;
	}

	OnRespMsg (static_cast<const rfa::message::RespMsg&>(msg), item_event.getClosure());
}

void
chok::consumer_t::OnRespMsg (
	const rfa::message::RespMsg&	reply_msg,
	void* closure
	)
{
	cumulative_stats_[CONSUMER_PC_RESPONSE_MSGS_RECEIVED]++;
	switch (reply_msg.getMsgModelType()) {
	case rfa::rdm::MMT_LOGIN:
		OnLoginResponse (reply_msg);
		break;

	case rfa::rdm::MMT_MARKET_PRICE:
		OnMarketPrice (reply_msg, closure);
		break;

	default:
		cumulative_stats_[CONSUMER_PC_RESPONSE_MSGS_DISCARDED]++;
		LOG(WARNING) << "Uncaught: " << reply_msg;
		break;
	}
}

void
chok::consumer_t::OnLoginResponse (
	const rfa::message::RespMsg&	reply_msg
	)
{
	cumulative_stats_[CONSUMER_PC_MMT_LOGIN_RESPONSE_RECEIVED]++;
	const rfa::common::RespStatus& respStatus = reply_msg.getRespStatus();

/* save state */
	stream_state_ = respStatus.getStreamState();
	data_state_   = respStatus.getDataState();

	switch (stream_state_) {
	case rfa::common::RespStatus::OpenEnum:
		switch (data_state_) {
		case rfa::common::RespStatus::OkEnum:
			OnLoginSuccess (reply_msg);
			break;

		case rfa::common::RespStatus::SuspectEnum:
			OnLoginSuspect (reply_msg);
			break;

		default:
			cumulative_stats_[CONSUMER_PC_MMT_LOGIN_RESPONSE_DISCARDED]++;
			LOG(WARNING) << "Uncaught: " << reply_msg;
			break;
		}
		break;

	case rfa::common::RespStatus::ClosedEnum:
		OnLoginClosed (reply_msg);
		break;

	default:
		cumulative_stats_[CONSUMER_PC_MMT_LOGIN_RESPONSE_DISCARDED]++;
		LOG(WARNING) << "Uncaught: " << reply_msg;
		break;
	}
}

void
chok::consumer_t::OnMarketPrice (
	const rfa::message::RespMsg&	reply_msg,
	void* closure
	)
{
	cumulative_stats_[CONSUMER_PC_MMT_MARKET_PRICE_RECEIVED]++;
	item_stream_t* item_stream = reinterpret_cast<item_stream_t*> (closure);
	CHECK (nullptr != item_stream);

	const auto now (boost::posix_time::second_clock::universal_time());
	item_stream->last_activity = now;
	item_stream->msg_count++;

	switch (reply_msg.getRespType()) {
	case rfa::message::RespMsg::RefreshEnum:
		item_stream->last_refresh = now;
		item_stream->refresh_received++;
		break;
	case rfa::message::RespMsg::StatusEnum:
		item_stream->last_status = now;
		item_stream->status_received++;
		break;
	case rfa::message::RespMsg::UpdateEnum:
		item_stream->last_update = now;
		item_stream->update_received++;
		break;
	default: break;
	}
}

/* 7.5.8.1.1 Login Success.
 * The stream state is OpenEnum one has received login permission from the
 * back-end infrastructure and the non-interactive provider can start to
 * publish data, including the service directory, dictionary, and other
 * response messages of different message model types.
 */
void
chok::consumer_t::OnLoginSuccess (
	const rfa::message::RespMsg&			login_msg
	)
{
	cumulative_stats_[CONSUMER_PC_MMT_LOGIN_SUCCESS]++;
	try {
/* SendDirectoryRequest(); */
		Resubscribe();
		LOG(INFO) << "Unmuting consumer.";
		is_muted_ = false;

/* ignore any error */
	} catch (const rfa::common::InvalidUsageException& e) {
		LOG(ERROR) << "MMT_DIRECTORY::InvalidUsageException: { StatusText: \"" << e.getStatus().getStatusText() << "\" }";
/* cannot publish until directory is sent. */
	} catch (const std::exception& e) {
		LOG(ERROR) << "Rfa::Exception: { "
			"\"What\": \"" << e.what() << "\" }";
	}
}

/* 7.5.8.1.2 Other Login States.
 * All connections are down. The application should stop publishing; it may
 * resume once the data state becomes OkEnum.
 */
void
chok::consumer_t::OnLoginSuspect (
	const rfa::message::RespMsg&			suspect_msg
	)
{
	cumulative_stats_[CONSUMER_PC_MMT_LOGIN_SUSPECT]++;
	is_muted_ = true;
}

/* 7.5.8.1.2 Other Login States.
 * The login failed, and the provider application failed to get permission
 * from the back-end infrastructure. In this case, the provider application
 * cannot start to publish data.
 */
void
chok::consumer_t::OnLoginClosed (
	const rfa::message::RespMsg&			logout_msg
	)
{
	cumulative_stats_[CONSUMER_PC_MMT_LOGIN_CLOSED]++;
	is_muted_ = true;
}

/* 7.5.8.2 Handling CmdError Events.
 * Represents an error Event that is generated during the submit() call on the
 * OMM non-interactive provider. This Event gives the provider application
 * access to the Cmd, CmdID, closure and OMMErrorStatus for the Cmd that
 * failed.
 */
void
chok::consumer_t::OnOMMCmdErrorEvent (
	const rfa::sessionLayer::OMMCmdErrorEvent& error
	)
{
	cumulative_stats_[CONSUMER_PC_OMM_CMD_ERRORS]++;
	LOG(ERROR) << "OMMCmdErrorEvent: { "
		  "\"CmdId\": " << error.getCmdID() <<
		", \"State\": " << error.getStatus().getState() <<
		", \"StatusCode\": " << error.getStatus().getStatusCode() <<
		", \"StatusText\": \"" << error.getStatus().getStatusText() << "\" }";
}

/* eof */
