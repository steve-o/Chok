/* RFA subscriber.
 *
 */

#ifndef __CHOK_HH__
#define __CHOK_HH__

#pragma once

#include <cstdint>

/* Boost noncopyable base class */
#include <boost/utility.hpp>

/* RFA 7.2 */
#include <rfa.hh>

#include "config.hh"
#include "consumer.hh"

namespace logging
{
	class LogEventProvider;
}

namespace chok
{
	class rfa_t;
	class consumer_t;

/* Basic example structure for application state of an item stream. */
	class subscription_stream_t : public item_stream_t
	{
	public:
		subscription_stream_t () :
			count (0)
		{
		}

		uint64_t	count;
	};

	class chok_t :
		boost::noncopyable
	{
	public:
		chok_t ();
		~chok_t();

/* Run the consumer with the given command-line parameters.
 * Returns the error code to be returned by main().
 */
		int run();
		void clear();

	private:

/* Run core event loop. */
		void mainLoop();

/* Application configuration. */
		config_t config_;

/* RFA context. */
		std::shared_ptr<rfa_t> rfa_;

/* RFA asynchronous event queue. */
		std::shared_ptr<rfa::common::EventQueue> event_queue_;

/* RFA logging */
		std::shared_ptr<logging::LogEventProvider> log_;

/* RFA consumer */
		std::shared_ptr<consumer_t> consumer_;
	
/* Item stream. */
		std::shared_ptr<subscription_stream_t> msft_stream_;

/* Update fields. */
		rfa::data::FieldList fields_;
	};

} /* namespace nezumi */

#endif /* __CHOK_HH__ */

/* eof */
