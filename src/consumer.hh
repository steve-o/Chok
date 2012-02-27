/* RFA consumer.
 */

#ifndef __CONSUMER_HH__
#define __CONSUMER_HH__

#pragma once

#include <cstdint>
#include <unordered_map>

/* Boost Posix Time */
#include "boost/date_time/posix_time/posix_time.hpp"

/* Boost noncopyable base class */
#include <boost/utility.hpp>

/* RFA 7.2 */
#include <rfa.hh>

#include "rfa.hh"
#include "config.hh"
#include "deleter.hh"

namespace chok
{
/* Performance Counters */
	enum {
		CONSUMER_PC_RFA_EVENTS_RECEIVED,
		CONSUMER_PC_RFA_EVENTS_DISCARDED,
		CONSUMER_PC_OMM_ITEM_EVENTS_RECEIVED,
		CONSUMER_PC_OMM_ITEM_EVENTS_DISCARDED,
		CONSUMER_PC_RESPONSE_MSGS_RECEIVED,
		CONSUMER_PC_RESPONSE_MSGS_DISCARDED,
		CONSUMER_PC_MMT_LOGIN_RESPONSE_RECEIVED,
		CONSUMER_PC_MMT_LOGIN_RESPONSE_DISCARDED,
		CONSUMER_PC_MMT_LOGIN_SUCCESS_RECEIVED,
		CONSUMER_PC_MMT_LOGIN_SUSPECT_RECEIVED,
		CONSUMER_PC_MMT_LOGIN_CLOSED_RECEIVED,
		CONSUMER_PC_OMM_CMD_ERRORS,
		CONSUMER_PC_MMT_LOGIN_VALIDATED,
		CONSUMER_PC_MMT_LOGIN_MALFORMED,
		CONSUMER_PC_MMT_LOGIN_SENT,
/* marker */
		CONSUMER_PC_MAX
	};

	class item_stream_t : boost::noncopyable
	{
	public:
/* Fixed name for this stream. */
		rfa::common::RFA_String rfa_name;
/* Subscription handle which is valid from login success to login close. */
		std::vector<rfa::common::Handle*> item_handle;
	};

	class session_t;

	class consumer_t :
		public rfa::common::Client,
		boost::noncopyable
	{
	public:
		consumer_t (const config_t& config, std::shared_ptr<rfa_t> rfa, std::shared_ptr<rfa::common::EventQueue> event_queue);
		~consumer_t();

		bool init() throw (rfa::common::InvalidConfigurationException, rfa::common::InvalidUsageException);

		bool createItemStream (const char* name, std::shared_ptr<item_stream_t> item_stream) throw (rfa::common::InvalidUsageException);

/* RFA event callback. */
		void processEvent (const rfa::common::Event& event);

		uint8_t getRwfMajorVersion() {
			return rwf_major_version_;
		}
		uint8_t getRwfMinorVersion() {
			return rwf_minor_version_;
		}

	private:
		void processOMMItemEvent (const rfa::sessionLayer::OMMItemEvent& event);
                void processRespMsg (const rfa::message::RespMsg& msg);
                void processLoginResponse (const rfa::message::RespMsg& msg);
                void processLoginSuccess (const rfa::message::RespMsg& msg);
                void processLoginSuspect (const rfa::message::RespMsg& msg);
                void processLoginClosed (const rfa::message::RespMsg& msg);
		void processOMMCmdErrorEvent (const rfa::sessionLayer::OMMCmdErrorEvent& event);

		bool sendLoginRequest() throw (rfa::common::InvalidUsageException);

		const config_t& config_;

/* RFA context. */
		std::shared_ptr<rfa_t> rfa_;

/* RFA asynchronous event queue. */
		std::shared_ptr<rfa::common::EventQueue> event_queue_;

/* RFA session defines one or more connections for horizontal scaling. */
		std::unique_ptr<rfa::sessionLayer::Session, internal::release_deleter> session_;

/* RFA OMM consumer interface. */
		std::unique_ptr<rfa::sessionLayer::OMMConsumer, internal::destroy_deleter> omm_consumer_;

/* RFA Error Item event consumer */
		rfa::common::Handle* error_item_handle_;
/* RFA Item event consumer */
		rfa::common::Handle* item_handle_;

/* Reuters Wire Format versions. */
		uint8_t rwf_major_version_;
		uint8_t rwf_minor_version_;

/* RFA will return a CmdError message if the provider application submits data
 * before receiving a login success message.  Mute downstream publishing until
 * permission is granted to submit data.
 */
		bool is_muted_;

/* Last RespStatus details. */
		int stream_state_;
		int data_state_;

/* Container of all item streams keyed by symbol name. */
		std::unordered_map<std::string, std::weak_ptr<item_stream_t>> directory_;

/** Performance Counters **/
		boost::posix_time::ptime last_activity_;
		uint32_t cumulative_stats_[CONSUMER_PC_MAX];
		uint32_t snap_stats_[CONSUMER_PC_MAX];
	};

} /* namespace chok */

#endif /* __CONSUMER_HH__ */

/* eof */
