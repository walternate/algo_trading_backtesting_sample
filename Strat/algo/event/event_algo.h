

#ifndef _STRAT_EVENT_ALGO
#define _STRAT_EVENT_ALGO

#include "algo.h"
#include "util.h"

#include <string>
#include <queue> 
#include <vector>
#include <list>
#include <future>

#include <boost/date_time.hpp>
#include <boost/algorithm/string.hpp> 

using std::string;

namespace strat{

	class event_algo : public algo {

	private:
		std::queue<boost::posix_time::ptime> _event_q;		

		///return size of the event_q
		size_t _push_obser_queue_if_event(tick crr_tick){

			//delete any event in queue that has passed
			while (!_event_q.empty() && _event_q.front() < crr_tick.time) {

				_event_q.pop();
			}

			if (!_event_q.empty() && _event_q.front() == crr_tick.time){

				_obser_tick_q.push(crr_tick);

				LOG("observed at tick " << crr_tick.time);

				_event_q.pop();
			}

			return _event_q.size();
		}

	protected:
		size_t _obser_win;
		size_t _hold_win;		
		double _run_sd;		
		std::queue<tick> _obser_tick_q;
		
		void _add_position(tick t, signal type, tick obser_t) {

			event_position pos;
			pos.open_tick = t;
			pos.obser_tick = obser_t;
			pos.type = type;
			_position = pos;

			LOG("opened position at tick " << t.time);
		}

		void _pop_obser_tick_queue(){

			if (!_obser_tick_q.empty()){

				LOG("removing observe " << _obser_tick_q.front().time);

				_obser_tick_q.pop();
			}
		}

		//csv file format: Date,Time,Time Zone,Currency,Event,Importance,Actual,Forecast,Previous
		//  file downloaded from http://www.dailyfx.com/calendar
		//  default filter cols_v. //Date,Time,Currency,Importance
		//  date time is in GMT time zone
		size_t _load_event(string event_f_path, const std::vector<int>& cols_filter){
			
			if (event_f_path.empty()){
				
				//LOG_SEV("passed in empty event path.",	logger::warning);
				return 0;
			}

			std::vector<std::vector<std::string>> event_v;
			util::read_csv(event_f_path, event_v, cols_filter);
			LOG_SEV("loaded event csv file. event count(all currencies): " << event_v.size() << ". path: " << event_f_path,
				logger::debug);

			boost::posix_time::ptime t;
			// assume event_v is in ascending order
			for (std::vector<std::vector<string>>::iterator it = event_v.begin(); it != event_v.end(); ++it){

				//add 0 if date is single digit
				string date = (*it)[0];
				if (date.length() == static_cast<size_t>(9))
					date = date.insert(8, "0");

				//work out year from file name by last 4 characters before extension. 
				//	e.g Calendar-2013.csv is file for year 2013
				string year_str = event_f_path.substr(event_f_path.size() - 8, 4);

				t = util::convert_to_dt(year_str + " " + date + " " + (*it)[1]);

				if (t.is_not_a_date_time()){

					LOG_SEV("failed to convert " << year_str + (*it)[0] + " " + (*it)[1], logger::warning);
					continue;
				}

				//note: summer start and end date is not same every year, below is based on 2014.
				//		but this is good enough approximation for back testing purpose
				auto summer_start = util::convert_to_dt(year_str + "0330", "%Y%m%d");
				auto summer_end = util::convert_to_dt(year_str + "1026", "%Y%m%d");
				if (t > summer_start && t < summer_end)
					t = t + boost::posix_time::hours(3);
				else
					t = t + boost::posix_time::hours(2);

				if (_event_q.empty() || _event_q.back() != t)	{

					string symbol = (*it)[2];
					boost::algorithm::to_lower(symbol);
					string importance = (*it)[3];
					boost::algorithm::to_lower(importance);

					throw("wrong below, show seperate symbol into base and quote");
					if ((symbol == _symbol || symbol == _symbol)
						&& (importance == "high" || importance == "medium")){

						_event_q.push(t);
					}
				}
			}

			LOG(_event_q.size() << " events enqueued");

			return _event_q.size();
		}

	public:

		event_algo(string symbol, 
			string event_f_path, size_t obser_win, size_t hold_win, double run_sd = 0.0003) :
			algo(symbol), _obser_win(obser_win), _hold_win(hold_win), _run_sd(run_sd){
			
			LOG_SEV("constructing event_algo. symbol:" << symbol <<
				" obser: " << obser_win << " hold : " << hold_win << " sd : " << run_sd, logger::debug);

			_load_event(event_f_path, std::vector<int>{ 0, 1, 3, 5 });
		};
				
		signal process_tick(const tick& crr_tick, position& close_pos, double& risk_lev,
			double stop_loss = -1, const double take_profit = -1) override{
			
			_push_obser_queue_if_event(crr_tick);

			if (_obser_tick_q.empty() && !has_open_position()){
			
				LOG_SEV("empty obser and position queue, abort process tick at tick " << crr_tick.time, logger::debug);
				return signal::NONE;
			}

			//process existing observe queue
			auto fut_get_sig = std::async([&]{

				return _get_signal_algo(crr_tick);
			});

			//process open positions
			auto fut_close_pos = std::async([&]{

				_close_position_algo(crr_tick, close_pos, stop_loss, take_profit);
			});

			fut_get_sig.wait();
			fut_close_pos.wait();

			signal ret_sig = fut_get_sig.get();

			if (close_pos.type != signal::NONE)	{
				
				LOG(" position(" << close_pos.open_tick.time << ") closed at tick "
					<< crr_tick.time);
			}

			return ret_sig;
		}

		size_t load_event(std::queue<boost::posix_time::ptime> event_q){

			_event_q = event_q;

			LOG(_event_q.size() << " events enqueued");

			return _event_q.size();
		}

		
#pragma region properties gets

		std::queue<boost::posix_time::ptime> get_event_queue() const{

			return _event_q;
		}

		std::queue<tick> get_obser_tick_queue() const{

			return _obser_tick_q;
		}

#pragma endregion
	};
}

#endif