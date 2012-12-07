/*
 * Note: this file originally auto-generated by mib2c using
 *  : mib2c.iterate.conf 17821 2009-11-11 09:00:00Z dts12 $
 */

#include "chokMIB.hh"

#include <list>

/* Boost threading. */
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>

/* redirect namespace pollution */
#define U64 __netsnmp_U64

#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

/* revert Net-snmp namespace pollution */
#undef U64
#define __netsnmp_LOG_EMERG	0
#define __netsnmp_LOG_ALERT	1
#define __netsnmp_LOG_CRIT	2
#define __netsnmp_LOG_ERR	3
#define __netsnmp_LOG_WARNING	4
#define __netsnmp_LOG_NOTICE	5
#define __netsnmp_LOG_INFO	6
#define __netsnmp_LOG_DEBUG	7
static_assert (__netsnmp_LOG_EMERG   == LOG_EMERG,   "LOG_EMERGE mismatch");
static_assert (__netsnmp_LOG_ALERT   == LOG_ALERT,   "LOG_ALERT mismatch");
static_assert (__netsnmp_LOG_CRIT    == LOG_CRIT,    "LOG_CRIT mismatch");
static_assert (__netsnmp_LOG_ERR     == LOG_ERR,     "LOG_ERR mismatch");
static_assert (__netsnmp_LOG_WARNING == LOG_WARNING, "LOG_WARNING mismatch");
static_assert (__netsnmp_LOG_NOTICE  == LOG_NOTICE,  "LOG_NOTICE mismatch");
static_assert (__netsnmp_LOG_INFO    == LOG_INFO,    "LOG_INFO mismatch");
static_assert (__netsnmp_LOG_DEBUG   == LOG_DEBUG,   "LOG_DEBUG mismatch");
#undef LOG_EMERG
#undef LOG_ALERT
#undef LOG_CRIT
#undef LOG_ERR
#undef LOG_WARNING
#undef LOG_NOTICE
#undef LOG_INFO
#undef LOG_DEBUG

#include "chromium/logging.hh"
#include "chok.hh"
#include "consumer.hh"

namespace chok {

/* http://en.wikipedia.org/wiki/Unix_epoch */
static const boost::posix_time::ptime kUnixEpoch (boost::gregorian::date (1970, 1, 1));

static int initialize_table_chokPerformanceTable(void);
static Netsnmp_Node_Handler chokPerformanceTable_handler;
static Netsnmp_First_Data_Point chokPerformanceTable_get_first_data_point;
static Netsnmp_Next_Data_Point chokPerformanceTable_get_next_data_point;
static Netsnmp_Free_Loop_Context chokPerformanceTable_free_loop_context;

static int initialize_table_chokSymbolTable(void);
static Netsnmp_Node_Handler chokSymbolTable_handler;
static Netsnmp_First_Data_Point chokSymbolTable_get_first_data_point;
static Netsnmp_Next_Data_Point chokSymbolTable_get_next_data_point;
static Netsnmp_Free_Loop_Context chokSymbolTable_free_loop_context;

/* Convert Posix time to Unix Epoch time.
 */
template< typename TimeT >
inline
TimeT
to_unix_epoch (
	const boost::posix_time::ptime t
	)
{
	return (t - boost::posix_time::ptime (kUnixEpoch)).total_seconds();
}

/* Context during a SNMP query, lock on global list of chok_t objects and iterator.
 */
class snmp_context_t
{
public:
	snmp_context_t (boost::shared_mutex& chok_lock_, std::list<chok::chok_t*>& chok_list_) :
		chok_lock (chok_lock_),
		chok_list (chok_list_),
		chok_it (chok_list.begin())
	{
	}

/* Plugins are owned by AE, locking is required. */
	boost::shared_lock<boost::shared_mutex> chok_lock;
	std::list<chok::chok_t*>& chok_list;
	std::list<chok::chok_t*>::iterator chok_it;
	boost::shared_lock<boost::shared_mutex> directory_lock;
	boost::unordered_map<std::string, std::weak_ptr<item_stream_t>>::iterator directory_it;

/* SNMP agent is not-reentrant, ignore locking. */
	static std::list<std::shared_ptr<snmp_context_t>> global_list;
};

std::list<std::shared_ptr<snmp_context_t>> snmp_context_t::global_list;

/* Initializes the chokMIB module.
 */
bool
init_chokMIB(void)
{
/* here we initialize all the tables we're planning on supporting */
	if (MIB_REGISTERED_OK != initialize_table_chokPerformanceTable()) {
		LOG(ERROR) << "chokPerformanceTable registration: see SNMP log for further details.";
		return false;
	}
	if (MIB_REGISTERED_OK != initialize_table_chokSymbolTable()) {
		LOG(ERROR) << "chokSymbolTable registration: see SNMP log for further details.";
		return false;
	}
	return true;
}

/* Initialize the chokPerformanceTable table by defining its contents and how it's structured
*/
static
int
initialize_table_chokPerformanceTable(void)
{
	DLOG(INFO) << "initialize_table_chokPerformanceTable()";

	static const oid chokPerformanceTable_oid[] = {1,3,6,1,4,1,67,4,1,2};
	const size_t chokPerformanceTable_oid_len = OID_LENGTH(chokPerformanceTable_oid);
	netsnmp_handler_registration* reg = nullptr;
	netsnmp_iterator_info* iinfo = nullptr;
	netsnmp_table_registration_info* table_info = nullptr;

	reg = netsnmp_create_handler_registration (
		"chokPerformanceTable",   chokPerformanceTable_handler,
		chokPerformanceTable_oid, chokPerformanceTable_oid_len,
		HANDLER_CAN_RONLY
		);
	if (nullptr == reg)
		goto error;

	table_info = SNMP_MALLOC_TYPEDEF (netsnmp_table_registration_info);
	if (nullptr == table_info)
		goto error;
	netsnmp_table_helper_add_indexes (table_info,
					  ASN_OCTET_STR,  /* index: chokApplicationId */
					  0);
	table_info->min_column = COLUMN_CHOKLASTACTIVITY;
	table_info->max_column = COLUMN_CHOKMMTMARKETPRICEREQUESTSENT;
    
	iinfo = SNMP_MALLOC_TYPEDEF( netsnmp_iterator_info );
	if (nullptr == iinfo)
		goto error;
	iinfo->get_first_data_point	= chokPerformanceTable_get_first_data_point;
	iinfo->get_next_data_point	= chokPerformanceTable_get_next_data_point;
	iinfo->free_loop_context_at_end = chokPerformanceTable_free_loop_context;
	iinfo->table_reginfo		= table_info;
    
	return netsnmp_register_table_iterator (reg, iinfo);

error:
	if (table_info && table_info->indexes)		/* table_data_free_func() is internal */
		snmp_free_var (table_info->indexes);
	SNMP_FREE (table_info);
	SNMP_FREE (iinfo);
	netsnmp_handler_registration_free (reg);
	return -1;
}

/* Example iterator hook routines - using 'get_next' to do most of the work
 */
static
netsnmp_variable_list*
chokPerformanceTable_get_first_data_point (
	void**			my_loop_context,	/* valid through one query of multiple "data points" */
	void**			my_data_context,	/* answer blob which is passed to handler() */
	netsnmp_variable_list*	put_index_data,		/* answer */
	netsnmp_iterator_info*	mydata			/* iinfo on init() */
	)
{
	assert (nullptr != my_loop_context);
	assert (nullptr != my_data_context);
	assert (nullptr != put_index_data);
	assert (nullptr != mydata);

	DLOG(INFO) << "chokPerformanceTable_get_first_data_point()";

/* Create our own context for this SNMP loop, lock on list follows lifetime of context */
	std::shared_ptr<snmp_context_t> context (new snmp_context_t (chok::chok_t::global_list_lock_, chok::chok_t::global_list_));
	if (!(bool)context || context->chok_list.empty()) {
		DLOG(INFO) << "No instances";
		return nullptr;
	}

/* Save context with NET-SNMP iterator. */
	*my_loop_context = context.get();
	snmp_context_t::global_list.push_back (std::move (context));

/* pass on for generic row access */
	return chokPerformanceTable_get_next_data_point(my_loop_context, my_data_context, put_index_data, mydata);
}

static
netsnmp_variable_list*
chokPerformanceTable_get_next_data_point (
	void**			my_loop_context,
	void**			my_data_context,
	netsnmp_variable_list*	put_index_data,
	netsnmp_iterator_info*	mydata
	)
{
	assert (nullptr != my_loop_context);
	assert (nullptr != my_data_context);
	assert (nullptr != put_index_data);
	assert (nullptr != mydata);

	DLOG(INFO) << "chokPerformanceTable_get_next_data_point()";

	snmp_context_t* context = static_cast<snmp_context_t*>(*my_loop_context);
	netsnmp_variable_list *idx = put_index_data;

/* end of data points */
	if (context->chok_it == context->chok_list.end()) {
		DLOG(INFO) << "End of instances.";
		return nullptr;
	}

/* this plugin instance as a data point */
	const chok::chok_t* chok = *context->chok_it++;

/* chokApplicationId */
	snmp_set_var_typed_value (idx, ASN_OCTET_STR, (const u_char*)chok->config_.application_id.c_str(), chok->config_.application_id.length());
        idx = idx->next_variable;

/* reference remains in list */
        *my_data_context = (void*)chok;
	return put_index_data;
}

static
void
chokPerformanceTable_free_loop_context (
	void*			my_loop_context,
	netsnmp_iterator_info*	mydata
	)
{
	assert (nullptr != my_loop_context);
	assert (nullptr != mydata);

	DLOG(INFO) << "chokPerformanceTable_free_loop_context()";

/* delete context and shared lock on global list of all chok objects */
	snmp_context_t* context = static_cast<snmp_context_t*>(my_loop_context);
/* I'm sure there must be a better method :-( */
	snmp_context_t::global_list.erase (std::remove_if (snmp_context_t::global_list.begin(),
		snmp_context_t::global_list.end(),
		[context](std::shared_ptr<snmp_context_t>& shared_context) -> bool {
			return shared_context.get() == context;
	}));
}

/* handles requests for the chokPerformanceTable table
 */
static
int
chokPerformanceTable_handler (
	netsnmp_mib_handler*		handler,
	netsnmp_handler_registration*	reginfo,
	netsnmp_agent_request_info*	reqinfo,
	netsnmp_request_info*		requests
	)
{
	assert (nullptr != handler);
	assert (nullptr != reginfo);
	assert (nullptr != reqinfo);
	assert (nullptr != requests);

	DLOG(INFO) << "chokPerformanceTable_handler()";

	switch (reqinfo->mode) {
        
/* Read-support (also covers GetNext requests) */

	case MODE_GET:
		for (netsnmp_request_info* request = requests;
		     request;
		     request = request->next)
		{
			const chok::chok_t* chok = static_cast<chok::chok_t*>(netsnmp_extract_iterator_context (request));
			if (nullptr == chok || !(bool)chok->consumer_) {
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}
			const auto consumer = chok->consumer_.get();

			netsnmp_variable_list* var = request->requestvb;
			netsnmp_table_request_info* table_info  = netsnmp_extract_table_info (request);
			if (nullptr == table_info) {
				snmp_log (__netsnmp_LOG_ERR, "chokPerformanceTable_handler: empty table request info.\n");
				continue;
			}
    
			switch (table_info->colnum) {

			case COLUMN_CHOKLASTACTIVITY:
				{
					union {
						uint32_t	uint_value;
						__time32_t	time32_t_value;
					} last_activity;
					last_activity.time32_t_value = (consumer->last_activity_ - kUnixEpoch).total_seconds();
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&last_activity.uint_value, sizeof (last_activity.uint_value));
				}
				break;

			case COLUMN_CHOKRFAEVENTSRECEIVED:
				{
					const unsigned events_received = consumer->cumulative_stats_[CONSUMER_PC_RFA_EVENTS_RECEIVED];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&events_received, sizeof (events_received));
				}
				break;

			case COLUMN_CHOKRFAEVENTSDISCARDED:
				{
					const unsigned events_discarded = consumer->cumulative_stats_[CONSUMER_PC_RFA_EVENTS_DISCARDED];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&events_discarded, sizeof (events_discarded));
				}
				break;

			case COLUMN_CHOKOMMITEMEVENTSRECEIVED:
				{
					const unsigned events_received = consumer->cumulative_stats_[CONSUMER_PC_OMM_ITEM_EVENTS_RECEIVED];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&events_received, sizeof (events_received));
				}
				break;

			case COLUMN_CHOKOMMITEMEVENTSDISCARDED:
				{
					const unsigned events_discarded = consumer->cumulative_stats_[CONSUMER_PC_OMM_ITEM_EVENTS_DISCARDED];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&events_discarded, sizeof (events_discarded));
				}
				break;

			case COLUMN_CHOKRESPONSEMSGSRECEIVED:
				{
					const unsigned msgs_received = consumer->cumulative_stats_[CONSUMER_PC_RESPONSE_MSGS_RECEIVED];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&msgs_received, sizeof (msgs_received));
				}
				break;

			case COLUMN_CHOKRESPONSEMSGSDISCARDED:
				{
					const unsigned msgs_discarded = consumer->cumulative_stats_[CONSUMER_PC_RESPONSE_MSGS_DISCARDED];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&msgs_discarded, sizeof (msgs_discarded));
				}
				break;

			case COLUMN_CHOKMMTLOGINRESPONSERECEIVED:
				{
					const unsigned login_received = consumer->cumulative_stats_[CONSUMER_PC_MMT_LOGIN_RESPONSE_RECEIVED];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&login_received, sizeof (login_received));
				}
				break;

			case COLUMN_CHOKMMTLOGINRESPONSEDISCARDED:
				{
					const unsigned login_discarded = consumer->cumulative_stats_[CONSUMER_PC_MMT_LOGIN_RESPONSE_DISCARDED];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&login_discarded, sizeof (login_discarded));
				}
				break;

			case COLUMN_CHOKMMTLOGINSUCCESS:
				{
					const unsigned login_success = consumer->cumulative_stats_[CONSUMER_PC_MMT_LOGIN_SUCCESS];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&login_success, sizeof (login_success));
				}
				break;

			case COLUMN_CHOKMMTLOGINSUSPECT:
				{
					const unsigned login_suspect = consumer->cumulative_stats_[CONSUMER_PC_MMT_LOGIN_SUSPECT];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&login_suspect, sizeof (login_suspect));
				}
				break;

			case COLUMN_CHOKMMTLOGINCLOSED:
				{
					const unsigned login_closed = consumer->cumulative_stats_[CONSUMER_PC_MMT_LOGIN_CLOSED];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&login_closed, sizeof (login_closed));
				}
				break;

			case COLUMN_CHOKOMMCOMMANDERRORS:
				{
					const unsigned cmd_errors = consumer->cumulative_stats_[CONSUMER_PC_OMM_CMD_ERRORS];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&cmd_errors, sizeof (cmd_errors));
				}
				break;

			case COLUMN_CHOKMMTLOGINVALIDATED:
				{
					const unsigned login_validated = consumer->cumulative_stats_[CONSUMER_PC_MMT_LOGIN_VALIDATED];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&login_validated, sizeof (login_validated));
				}
				break;

			case COLUMN_CHOKMMTLOGINMALFORMED:
				{
					const unsigned login_malformed = consumer->cumulative_stats_[CONSUMER_PC_MMT_LOGIN_MALFORMED];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&login_malformed, sizeof (login_malformed));
				}
				break;

			case COLUMN_CHOKMMTLOGINEXCEPTION:
				{
					const unsigned login_exception = consumer->cumulative_stats_[CONSUMER_PC_MMT_LOGIN_EXCEPTION];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&login_exception, sizeof (login_exception));
				}
				break;

			case COLUMN_CHOKMMTLOGINSENT:
				{
					const unsigned login_sent = consumer->cumulative_stats_[CONSUMER_PC_MMT_LOGIN_SENT];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&login_sent, sizeof (login_sent));
				}
				break;

			case COLUMN_CHOKMMTMARKETPRICERECEIVED:
				{
					const unsigned msgs_received = consumer->cumulative_stats_[CONSUMER_PC_MMT_MARKET_PRICE_RECEIVED];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&msgs_received, sizeof (msgs_received));
				}
				break;

			case COLUMN_CHOKMMTMARKETPRICEREQUESTVALIDATED:
				{
					const unsigned msgs_sent = consumer->cumulative_stats_[CONSUMER_PC_MMT_MARKET_PRICE_REQUEST_VALIDATED];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&msgs_sent, sizeof (msgs_sent));
				}
				break;

			case COLUMN_CHOKMMTMARKETPRICEREQUESTMALFORMED:
				{
					const unsigned msgs_sent = consumer->cumulative_stats_[CONSUMER_PC_MMT_MARKET_PRICE_REQUEST_MALFORMED];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&msgs_sent, sizeof (msgs_sent));
				}
				break;

			case COLUMN_CHOKMMTMARKETPRICEREQUESTEXCEPTION:
				{
					const unsigned msgs_sent = consumer->cumulative_stats_[CONSUMER_PC_MMT_MARKET_PRICE_REQUEST_EXCEPTION];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&msgs_sent, sizeof (msgs_sent));
				}
				break;

			case COLUMN_CHOKMMTMARKETPRICEREQUESTSENT:
				{
					const unsigned msgs_sent = consumer->cumulative_stats_[CONSUMER_PC_MMT_MARKET_PRICE_REQUEST_SENT];
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&msgs_sent, sizeof (msgs_sent));
				}
				break;

			default:
				snmp_log (__netsnmp_LOG_ERR, "chokPerformanceTable_handler: unknown column.\n");
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;

	default:
		snmp_log (__netsnmp_LOG_ERR, "chokPerformanceTable_handler: unsupported mode.\n");
		break;
    }

    return SNMP_ERR_NOERROR;
}

/* Initialize the chokSymbolTable table by defining its contents and how it's structured.
 */
static
int
initialize_table_chokSymbolTable(void)
{
	DLOG(INFO) << "initialize_table_chokSymbolTable()";

	static const oid chokSymbolTable_oid[] = {1,3,6,1,4,1,67,4,1,3};
	const size_t chokSymbolTable_oid_len = OID_LENGTH (chokSymbolTable_oid);
	netsnmp_handler_registration* reg = nullptr;
	netsnmp_iterator_info* iinfo = nullptr;
	netsnmp_table_registration_info* table_info = nullptr;

	reg = netsnmp_create_handler_registration (
		"chokSymbolTable",	chokSymbolTable_handler,
		chokSymbolTable_oid,	chokSymbolTable_oid_len,
		HANDLER_CAN_RONLY
		);
	if (nullptr == reg)
		goto error;

	table_info = SNMP_MALLOC_TYPEDEF (netsnmp_table_registration_info);
	if (nullptr == table_info)
		goto error;
	netsnmp_table_helper_add_indexes (table_info,
					  ASN_OCTET_STR,  /* index: chokApplicationId */
					  ASN_OCTET_STR,  /* index: chokServiceName */
					  ASN_OCTET_STR,  /* index: chokItemName */
					  0);
	table_info->min_column = COLUMN_CHOKSYMBOLMSGSRECEIVED;
	table_info->max_column = COLUMN_CHOKSYMBOLLASTUPDATE;
    
	iinfo = SNMP_MALLOC_TYPEDEF (netsnmp_iterator_info);
	if (nullptr == iinfo)
		goto error;
	iinfo->get_first_data_point	= chokSymbolTable_get_first_data_point;
	iinfo->get_next_data_point	= chokSymbolTable_get_next_data_point;
	iinfo->free_loop_context_at_end	= chokSymbolTable_free_loop_context;
	iinfo->table_reginfo		= table_info;
    
	return netsnmp_register_table_iterator (reg, iinfo);

error:
	if (table_info && table_info->indexes)		/* table_data_free_func() is internal */
		snmp_free_var (table_info->indexes);
	SNMP_FREE (table_info);
	SNMP_FREE (iinfo);
	netsnmp_handler_registration_free (reg);
	return -1;
}

/* Example iterator hook routines - using 'get_next' to do most of the work */
static 
netsnmp_variable_list*
chokSymbolTable_get_first_data_point (
	void**			my_loop_context,	/* valid through one query of multiple "data points" */
	void**			my_data_context,	/* answer blob which is passed to handler() */
	netsnmp_variable_list*	put_index_data,		/* answer */
	netsnmp_iterator_info*	mydata			/* iinfo on init() */
	)
{
	assert (nullptr != my_loop_context);
	assert (nullptr != my_data_context);
	assert (nullptr != put_index_data);
	assert (nullptr != mydata);

	DLOG(INFO) << "chokSymbolTable_get_first_data_point()";

/* Create our own context for this SNMP loop, lock on list follows lifetime of context */
	std::shared_ptr<snmp_context_t> context (new snmp_context_t (chok::chok_t::global_list_lock_, chok::chok_t::global_list_));
	if (!(bool)context || context->chok_list.empty()) {
		DLOG(INFO) << "No instances";
		return nullptr;
	}

/* Find first node, through all application instances. */
	for (context->chok_it = context->chok_list.begin(); context->chok_it != context->chok_list.end(); ++(context->chok_it))
	{
		boost::shared_lock<boost::shared_mutex> lock ((*context->chok_it)->consumer_->directory_lock_);
/* and through all instruments for each application consumer. */
		context->directory_it = (*context->chok_it)->consumer_->directory_.begin();
		if (context->directory_it != (*context->chok_it)->consumer_->directory_.end()) {
			context->directory_lock.swap (lock);
			break;
		}
	}

/* no node found. */
	if (context->chok_it == context->chok_list.end() ||
	    context->directory_it == (*context->chok_it)->consumer_->directory_.end()) {
		DLOG(INFO) << "No consumer instruments.";
		return nullptr;
	}

/* Save context with NET-SNMP iterator. */
	*my_loop_context = context.get();
	snmp_context_t::global_list.push_back (std::move (context));

/* pass on for generic row access */
	return chokSymbolTable_get_next_data_point (my_loop_context, my_data_context, put_index_data,  mydata);
}

static
netsnmp_variable_list*
chokSymbolTable_get_next_data_point (
	void**			my_loop_context,
	void**			my_data_context,
	netsnmp_variable_list*	put_index_data,
	netsnmp_iterator_info*	mydata
	)
{
	assert (nullptr != my_loop_context);
	assert (nullptr != my_data_context);
	assert (nullptr != put_index_data);
	assert (nullptr != mydata);

	DLOG(INFO) << "chokSymbolTable_get_next_data_point()";

	snmp_context_t* context = static_cast<snmp_context_t*>(*my_loop_context);
	netsnmp_variable_list *idx = put_index_data;

/* end of data points */
	if (context->chok_it == context->chok_list.end()) {
		DLOG(INFO) << "End of plugin instances.";
		return nullptr;
	}
	if (context->directory_it == (*context->chok_it)->consumer_->directory_.end()) {
		DLOG(INFO) << "End of consumer instruments.";
		return nullptr;
	}

/* this application instance as a data point */
	auto sp = (context->directory_it->second).lock();
	const chok::item_stream_t* item_stream = sp.get();
	const chok::chok_t* chok = *context->chok_it;

/* chokApplicationId */
	snmp_set_var_typed_value (idx, ASN_OCTET_STR, (const u_char*)chok->config_.application_id.c_str(), chok->config_.application_id.length());
        idx = idx->next_variable;

/* chokServiceName */
	snmp_set_var_typed_value (idx, ASN_OCTET_STR, (const u_char*)item_stream->rfa_service_name.c_str(), item_stream->rfa_service_name.length());
        idx = idx->next_variable;

/* chokItemName */
	snmp_set_var_typed_value (idx, ASN_OCTET_STR, (const u_char*)item_stream->rfa_item_name.c_str(), item_stream->rfa_item_name.length());
        idx = idx->next_variable;

/* hunt for next valid node */
	while (++(context->directory_it) == (*context->chok_it)->consumer_->directory_.end()) {
		context->directory_lock.unlock ();
		if (++(context->chok_it) == context->chok_list.end()) {
			break;
		}
		boost::shared_lock<boost::shared_mutex> lock ((*context->chok_it)->consumer_->directory_lock_);
		context->directory_lock.swap (lock);
		context->directory_it = (*context->chok_it)->consumer_->directory_.begin();
	}

/* reference remains in list */
        *my_data_context = (void*)item_stream;
        return put_index_data;
}

static
void
chokSymbolTable_free_loop_context (
	void*			my_loop_context,
	netsnmp_iterator_info*	mydata
	)
{
	assert (nullptr != my_loop_context);
	assert (nullptr != mydata);

	DLOG(INFO) << "chokSymbolTable_free_loop_context()";

/* delete context and shared lock on global list of all chok objects */
	snmp_context_t* context = static_cast<snmp_context_t*>(my_loop_context);
/* I'm sure there must be a better method :-( */
	snmp_context_t::global_list.erase (std::remove_if (snmp_context_t::global_list.begin(),
		snmp_context_t::global_list.end(),
		[context](std::shared_ptr<snmp_context_t>& shared_context) -> bool {
			return shared_context.get() == context;
	}));
}

/* handles requests for the chokSymbolTable table
 */
static
int
chokSymbolTable_handler (
	netsnmp_mib_handler*		handler,
	netsnmp_handler_registration*	reginfo,
	netsnmp_agent_request_info*	reqinfo,
	netsnmp_request_info*		requests
	)
{
	assert (nullptr != handler);
	assert (nullptr != reginfo);
	assert (nullptr != reqinfo);
	assert (nullptr != requests);

	DLOG(INFO) << "chokSymbolTable_handler()";

	switch (reqinfo->mode) {

/* Read-support (also covers GetNext requests) */

	case MODE_GET:
		for (netsnmp_request_info* request = requests;
		     request;
		     request = request->next)
		{
			const chok::item_stream_t* item_stream = static_cast<chok::item_stream_t*>(netsnmp_extract_iterator_context (request));
			if (nullptr == item_stream) {
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHINSTANCE);
				continue;
			}

			netsnmp_variable_list* var = request->requestvb;
			netsnmp_table_request_info* table_info = netsnmp_extract_table_info (request);
			if (nullptr == table_info) {
				snmp_log (__netsnmp_LOG_ERR, "chokSymbolTable_handler: empty table request info.\n");
				continue;
			}
    
			switch (table_info->colnum) {					
/* all */
			case COLUMN_CHOKSYMBOLMSGSRECEIVED:
				{
					const unsigned msgs_received = (unsigned)item_stream->msg_count;
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&msgs_received, sizeof (msgs_received));
				}
				break;

			case COLUMN_CHOKSYMBOLLASTACTIVITY:
				{
					union {
						uint32_t	uint_value;
						__time32_t	time32_t_value;
					} last_activity;
					last_activity.time32_t_value = (item_stream->last_activity - kUnixEpoch).total_seconds();
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&last_activity.uint_value, sizeof (last_activity.uint_value));
				}
				break;

/* refresh-only */
			case COLUMN_CHOKSYMBOLREFRESHMSGSRECEIVED:
				{
					const unsigned msgs_received = (unsigned)item_stream->refresh_received;
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&msgs_received, sizeof (msgs_received));
				}
				break;

			case COLUMN_CHOKSYMBOLLASTREFRESH:
				{
					union {
						uint32_t	uint_value;
						__time32_t	time32_t_value;
					} last_refresh;
					if (item_stream->last_refresh.is_not_a_date_time())
						last_refresh.uint_value = 0;
					else
						last_refresh.time32_t_value = (item_stream->last_refresh - kUnixEpoch).total_seconds();
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&last_refresh.uint_value, sizeof (last_refresh.uint_value));
				}
				break;

/* status-only */
			case COLUMN_CHOKSYMBOLSTATUSMSGSRECEIVED:
				{
					const unsigned msgs_received = (unsigned)item_stream->status_received;
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&msgs_received, sizeof (msgs_received));
				}
				break;

			case COLUMN_CHOKSYMBOLLASTSTATUS:
				{
					union {
						uint32_t	uint_value;
						__time32_t	time32_t_value;
					} last_status;
					if (item_stream->last_status.is_not_a_date_time())
						last_status.uint_value = 0;
					else
						last_status.time32_t_value = (item_stream->last_status - kUnixEpoch).total_seconds();
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&last_status.uint_value, sizeof (last_status.uint_value));
				}
				break;

/* update-only */
			case COLUMN_CHOKSYMBOLUPDATEMSGSRECEIVED:
				{
					const unsigned msgs_received = (unsigned)item_stream->update_received;
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&msgs_received, sizeof (msgs_received));
				}
				break;

			case COLUMN_CHOKSYMBOLLASTUPDATE:
				{
					union {
						uint32_t	uint_value;
						__time32_t	time32_t_value;
					} last_update;
					if (item_stream->last_update.is_not_a_date_time())
						last_update.uint_value = 0;
					else
						last_update.time32_t_value = (item_stream->last_update - kUnixEpoch).total_seconds();
					snmp_set_var_typed_value (var, ASN_COUNTER, /* ASN_COUNTER32 */
						(const u_char*)&last_update.uint_value, sizeof (last_update.uint_value));
				}
				break;


			default:
				snmp_log (__netsnmp_LOG_ERR, "chokSymbolTable_handler: unknown column.\n");
				netsnmp_set_request_error (reqinfo, request, SNMP_NOSUCHOBJECT);
				break;
			}
		}
		break;

	default:
		snmp_log (__netsnmp_LOG_ERR, "chokSymbolTable_handler: unnsupported mode.\n");
		break;
	}
	return SNMP_ERR_NOERROR;
}

} /* namespace chok */

/* eof */