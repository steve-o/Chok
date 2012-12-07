/* RFA subscriber.
 *
 */

#ifndef __CHOK_HH__
#define __CHOK_HH__
#pragma once

#include <cstdint>

/* Boost noncopyable base class */
#include <boost/utility.hpp>

/* Boost threading. */
#include <boost/thread.hpp>

/* RFA 7.2 */
#include <rfa/rfa.hh>

#include "config.hh"
#include "consumer.hh"

namespace logging
{
namespace rfa
{
	class LogEventProvider;
}
}

namespace chok
{
	class rfa_t;
	class consumer_t;
	class snmp_agent_t;

/* Basic example structure for application state of an item stream. */
	class subscription_stream_t : public item_stream_t
	{
	public:
		subscription_stream_t ()
		{
		}
	};

	class chok_t :
		boost::noncopyable
	{
	public:
		chok_t ();
		~chok_t();

		bool Init();
/* Run the consumer with the given command-line parameters.
 * Returns the error code to be returned by main().
 */
		int Run();
		void Clear();

/* Global list of all application instances. */
		static std::list<chok_t*> global_list_;
		static boost::shared_mutex global_list_lock_;

	private:
/* Run core event loop. */
		void MainLoop();

/* Application configuration. */
		config_t config_;

/* SNMP implant. */
		std::unique_ptr<snmp_agent_t> snmp_agent_;
		friend class snmp_agent_t;

#ifdef CHOKMIB_H
		friend Netsnmp_Next_Data_Point chokPerformanceTable_get_next_data_point;
		friend Netsnmp_Node_Handler chokPerformanceTable_handler;

		friend Netsnmp_First_Data_Point chokSymbolTable_get_first_data_point;
		friend Netsnmp_Next_Data_Point chokSymbolTable_get_next_data_point;
#endif /* CHOKMIB_H */


/* RFA context. */
		std::shared_ptr<rfa_t> rfa_;

/* RFA asynchronous event queue. */
		std::shared_ptr<rfa::common::EventQueue> event_queue_;

/* RFA logging */
		std::shared_ptr<logging::rfa::LogEventProvider> log_;

/* RFA consumer */
		std::shared_ptr<consumer_t> consumer_;
	
/* Item stream. */
		std::list<std::shared_ptr<subscription_stream_t>> streams_;

/* Update fields. */
		rfa::data::FieldList fields_;
	};

} /* namespace chok */

#endif /* __CHOK_HH__ */

/* eof */