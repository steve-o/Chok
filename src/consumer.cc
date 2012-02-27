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
chok::consumer_t::init()
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
	LOG(INFO) << "RFA: { productVersion: \"" << rfa::common::Context::getRFAVersionInfo()->getProductVersion() << "\" }";

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

	return sendLoginRequest();
}

/* 7.3.5.3 Making a Login Request	
 * A Login request message is encoded and sent by OMM Consumer and OMM non-
 * interactive provider applications.
 */
bool
chok::consumer_t::sendLoginRequest()
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
	RFA_String warningText;
	const uint8_t validation_status = request.validateMsg (&warningText);
	if (rfa::message::MsgValidationWarning == validation_status) {
		LOG(WARNING) << "MMT_LOGIN::validateMsg: { warningText: \"" << warningText << "\" }";
		cumulative_stats_[CONSUMER_PC_MMT_LOGIN_MALFORMED]++;
	} else {
		assert (rfa::message::MsgValidationOk == validation_status);
		cumulative_stats_[CONSUMER_PC_MMT_LOGIN_VALIDATED]++;
	}

/* Not saving the returned handle as we will destroy the provider to logout,
 * reference:
 * 7.4.10.6 Other Cleanup
 * Note: The application may call destroy() on an Event Source without having
 * closed all Event Streams. RFA will internally unregister all open Event
 * Streams in this case.
 */
	VLOG(3) << "Registering OMM item interest.";
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
	LOG(INFO) << "RWF: { MajorVersion: " << (unsigned)rwf_major_version_
		<< ", MinorVersion: " << (unsigned)rwf_minor_version_ << " }";
	return true;
}

/* Create an item stream for a given symbol name.  The Item Stream maintains
 * the provider state on behalf of the application.
 */
bool
chok::consumer_t::createItemStream (
	const char* name,
	std::shared_ptr<item_stream_t> item_stream
	)
{
	VLOG(4) << "Creating item stream for RIC \"" << name << "\".";
	item_stream->rfa_name.set (name, 0, true);
	if (!is_muted_) {
	} else {
	}
	const std::string key (name);
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
		processOMMItemEvent (static_cast<const rfa::sessionLayer::OMMItemEvent&>(event_));
		break;

        case rfa::sessionLayer::OMMCmdErrorEventEnum:
                processOMMCmdErrorEvent (static_cast<const rfa::sessionLayer::OMMCmdErrorEvent&>(event_));
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
chok::consumer_t::processOMMItemEvent (
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

	processRespMsg (static_cast<const rfa::message::RespMsg&>(msg));
}

void
chok::consumer_t::processRespMsg (
	const rfa::message::RespMsg&	reply_msg
	)
{
	cumulative_stats_[CONSUMER_PC_RESPONSE_MSGS_RECEIVED]++;
/* Verify event is a login response event */
	if (rfa::rdm::MMT_LOGIN != reply_msg.getMsgModelType()) {
		cumulative_stats_[CONSUMER_PC_RESPONSE_MSGS_DISCARDED]++;
		LOG(WARNING) << "Uncaught: " << reply_msg;
		return;
	}

	cumulative_stats_[CONSUMER_PC_MMT_LOGIN_RESPONSE_RECEIVED]++;
	const rfa::common::RespStatus& respStatus = reply_msg.getRespStatus();

/* save state */
	stream_state_ = respStatus.getStreamState();
	data_state_   = respStatus.getDataState();

	switch (stream_state_) {
	case rfa::common::RespStatus::OpenEnum:
		switch (data_state_) {
		case rfa::common::RespStatus::OkEnum:
			processLoginSuccess (reply_msg);
			break;

		case rfa::common::RespStatus::SuspectEnum:
			processLoginSuspect (reply_msg);
			break;

		default:
			cumulative_stats_[CONSUMER_PC_MMT_LOGIN_RESPONSE_DISCARDED]++;
			LOG(WARNING) << "Uncaught: " << reply_msg;
			break;
		}
		break;

	case rfa::common::RespStatus::ClosedEnum:
		processLoginClosed (reply_msg);
		break;

	default:
		cumulative_stats_[CONSUMER_PC_MMT_LOGIN_RESPONSE_DISCARDED]++;
		LOG(WARNING) << "Uncaught: " << reply_msg;
		break;
	}
}

/* 7.5.8.1.1 Login Success.
 * The stream state is OpenEnum one has received login permission from the
 * back-end infrastructure and the non-interactive provider can start to
 * publish data, including the service directory, dictionary, and other
 * response messages of different message model types.
 */
void
chok::consumer_t::processLoginSuccess (
	const rfa::message::RespMsg&			login_msg
	)
{
	cumulative_stats_[CONSUMER_PC_MMT_LOGIN_SUCCESS_RECEIVED]++;
	try {
		LOG(INFO) << "Unmuting consumer.";
		is_muted_ = false;

/* ignore any error */
	} catch (rfa::common::InvalidUsageException& e) {
		LOG(ERROR) << "MMT_DIRECTORY::InvalidUsageException: { StatusText: \"" << e.getStatus().getStatusText() << "\" }";
/* cannot publish until directory is sent. */
		return;
	}
}

/* 7.5.8.1.2 Other Login States.
 * All connections are down. The application should stop publishing; it may
 * resume once the data state becomes OkEnum.
 */
void
chok::consumer_t::processLoginSuspect (
	const rfa::message::RespMsg&			suspect_msg
	)
{
	cumulative_stats_[CONSUMER_PC_MMT_LOGIN_SUSPECT_RECEIVED]++;
	is_muted_ = true;
}

/* 7.5.8.1.2 Other Login States.
 * The login failed, and the provider application failed to get permission
 * from the back-end infrastructure. In this case, the provider application
 * cannot start to publish data.
 */
void
chok::consumer_t::processLoginClosed (
	const rfa::message::RespMsg&			logout_msg
	)
{
	cumulative_stats_[CONSUMER_PC_MMT_LOGIN_CLOSED_RECEIVED]++;
	is_muted_ = true;
}

/* 7.5.8.2 Handling CmdError Events.
 * Represents an error Event that is generated during the submit() call on the
 * OMM non-interactive provider. This Event gives the provider application
 * access to the Cmd, CmdID, closure and OMMErrorStatus for the Cmd that
 * failed.
 */
void
chok::consumer_t::processOMMCmdErrorEvent (
	const rfa::sessionLayer::OMMCmdErrorEvent& error
	)
{
	cumulative_stats_[CONSUMER_PC_OMM_CMD_ERRORS]++;
	LOG(ERROR) << "OMMCmdErrorEvent: { "
		"CmdId: " << error.getCmdID() <<
		", State: " << error.getStatus().getState() <<
		", StatusCode: " << error.getStatus().getStatusCode() <<
		", StatusText: \"" << error.getStatus().getStatusText() << "\" }";
}

/* eof */
