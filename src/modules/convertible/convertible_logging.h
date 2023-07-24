//
// Created by raffaele on 29/05/19.
//

#ifndef E_GADGET_CONVERTIBLE_CONVERTIBLE_LOGGING_H
#define E_GADGET_CONVERTIBLE_CONVERTIBLE_LOGGING_H

#undef CRIT
#undef ERR
#undef WARN
#undef INF
#undef DBG

extern int _convertible_log_dom;
#define CRIT(...)     EINA_LOG_DOM_CRIT(_convertible_log_dom, __VA_ARGS__)
#define ERR(...)      EINA_LOG_DOM_ERR(_convertible_log_dom, __VA_ARGS__)
#define WARN(...)      EINA_LOG_DOM_WARN(_convertible_log_dom, __VA_ARGS__)
#define INF(...)      EINA_LOG_DOM_INFO(_convertible_log_dom, __VA_ARGS__)
#define DBG(...)      EINA_LOG_DOM_DBG(_convertible_log_dom, __VA_ARGS__)


#endif //E_GADGET_CONVERTIBLE_CONVERTIBLE_LOGGING_H
