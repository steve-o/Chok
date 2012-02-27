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
		CONSUMER_PC_NA,
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
		boost::noncopyable
	{
	public:
		consumer_t (const config_t& config, std::shared_ptr<rfa_t> rfa, std::shared_ptr<rfa::common::EventQueue> event_queue);
		~consumer_t();

		bool init() throw (rfa::common::InvalidConfigurationException, rfa::common::InvalidUsageException);

		bool createItemStream (const char* name, std::shared_ptr<item_stream_t> item_stream) throw (rfa::common::InvalidUsageException);

		uint8_t getRwfMajorVersion() {
			return min_rwf_major_version_;
		}
		uint8_t getRwfMinorVersion() {
			return min_rwf_minor_version_;
		}

	private:
		const config_t& config_;

/* Reuters Wire Format versions. */
		uint8_t min_rwf_major_version_;
		uint8_t min_rwf_minor_version_;

		std::vector<std::unique_ptr<session_t>> sessions_;

/* Container of all item streams keyed by symbol name. */
		std::unordered_map<std::string, std::weak_ptr<item_stream_t>> directory_;

		friend session_t;

/** Performance Counters **/
		boost::posix_time::ptime last_activity_;
		uint32_t cumulative_stats_[CONSUMER_PC_MAX];
		uint32_t snap_stats_[CONSUMER_PC_MAX];
	};

} /* namespace chok */

#endif /* __CONSUMER_HH__ */

/* eof */
