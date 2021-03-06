#include "stdafx.h"

#ifdef _DEBUG1

#include "CppUnitTest.h"

#include "util.h"
#include "algo\event\event_long_short.h"
#include "algo\event\event_algo_ma.h"
#include "logger.h"
#include "indicator\sma.h"
#include "trend.h"
#include "optimizer\optimizer_genetic.h"
#include "algo\algo_bollinger.h"
#include "algo\algo_dayrange.h"

#include <iostream>
#include <thread>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Strat
{
	TEST_CLASS(unit_test)
	{
	public:
		
		TEST_METHOD(util_read_csv)
		{
			std::vector<std::vector<std::string>> vec;
			std::vector<int> cols_v{ 0, 1 };
			util::read_csv("../../test_files/Calendar-11-24-2013.csv", vec, cols_v);
			
			size_t size = 28;
			Assert::AreEqual(size, vec.size());
			size = 2;
			Assert::AreEqual(size, vec[0].size());

			Assert::AreEqual(std::string("Mon Nov 25"), vec[9][0]);
			Assert::AreEqual(std::string("23:50"), vec[9][1]);
		}

		TEST_METHOD(util_convert_to_dt)
		{
			boost::posix_time::ptime t1 = 
				boost::posix_time::time_from_string(std::string("2013-11-25 23:50:00.000"));
			boost::posix_time::ptime t2 = util::convert_to_dt(std::string("2013 Mon Nov 25 23:50"));
			Assert::IsTrue(t1 == t2);
		}

		TEST_METHOD(event_long_short_constructor)
		{
			strat::event_long_short algo("eurusd", "../../test_files/Calendar-11-24-2013.csv"
				, 5, 15, 0.0003);

			std::queue<boost::posix_time::ptime> event_q = algo.get_event_queue();

			boost::posix_time::ptime t =
				boost::posix_time::time_from_string(std::string("2013-11-25 04:00:00.000"));
			Assert::IsTrue(t == event_q.front());

			event_q.pop();
			event_q.pop();
			t = boost::posix_time::time_from_string(std::string("2013-11-25 15:30:00.000"));
			Assert::IsTrue(t == event_q.front());
		}

		TEST_METHOD(util_read_tick_csv)
		{
			std::vector<strat::tick> tick_vec;
			//util::read_tick_csv("../../test_files/EURUSD_min_11-24-2013.csv", tick_vec);

			size_t size = 2938;
			Assert::AreEqual(tick_vec.size(), size);
			boost::posix_time::ptime t =
				boost::posix_time::time_from_string(std::string("2013-11-24 23:10:00.000"));
			Assert::IsTrue(tick_vec[9].time == t);
			Assert::AreEqual(tick_vec[9].last, 1.3549);

		}

		TEST_METHOD(event_long_short_process_tick)
		{
			strat::event_long_short algo("eurusd", "../../test_files/Calendar-11-24-2013.csv"
				, 5, 15, 0.0003, true);

			std::vector<strat::tick> tick_vec;
			//util::read_tick_csv("../../test_files/EURUSD_min_11-24-2013.csv", tick_vec);

			strat::position close_pos;
			std::queue<strat::tick> obser_q;
			strat::signal sig = strat::signal::NONE;

			for (int i = 0; i < 10; i++){
				algo.process_tick(tick_vec[i], close_pos);
				obser_q = algo.get_obser_tick_queue();
				Assert::IsTrue(obser_q.empty());
				Assert::IsTrue(strat::signal::NONE == sig);
			}

			//index no here = 'row index in file' - 2 (exclude header and c++ is 0 start index)
			sig = algo.process_tick(tick_vec[298], close_pos);
			obser_q = algo.get_obser_tick_queue();
			Assert::IsTrue(obser_q.empty());
			Assert::IsTrue(close_pos.type == strat::signal::NONE);
			Assert::IsTrue(strat::signal::NONE == sig);

			sig = algo.process_tick(tick_vec[299], close_pos);
			obser_q = algo.get_obser_tick_queue();
			size_t size = 1;
			Assert::AreEqual(obser_q.size(), size);
			Assert::IsTrue(close_pos.type == strat::signal::NONE);
			Assert::IsTrue(strat::signal::NONE == sig);

			sig = algo.process_tick(tick_vec[304], close_pos);
			obser_q = algo.get_obser_tick_queue();
			Assert::IsTrue(close_pos.type == strat::signal::NONE);
			Assert::IsTrue(strat::signal::BUY == sig);
			Assert::IsTrue(algo.has_open_position());
			strat::position pos = algo.get_position();
			Assert::IsTrue(pos.open_tick.last == 1.3540);

			sig = algo.process_tick(tick_vec[319], close_pos);
			obser_q = algo.get_obser_tick_queue();
			Assert::IsTrue(obser_q.empty());
			Assert::IsTrue(strat::signal::NONE == sig);
			Assert::IsTrue(close_pos.type != strat::signal::NONE);
			Assert::IsTrue(!algo.has_open_position());
		}

#pragma region test log

		static void test_log(std::string t){

			const boost::posix_time::ptime now =
				boost::posix_time::second_clock::local_time();

			LOG(t << " A normal severity message, will not pass to the file at " << now);
			LOG_SEV(t << " A warning severity message, will pass to the file at " << now,
				logger::warning);
			LOG_SEV(t << " A error severity message, will pass to the file at " << now,
				logger::error);

			std::list<strat::position> pos;
			strat::position p;
			
			strat::tick tick1;
			tick1.time = now;
			tick1.last = 1.006;

			p.open_tick = tick1;
			p.close_tick = tick1;

			pos.push_back(p);
			pos.push_back(p);

			LOG_POSITIONS(pos);
		}

		//https://github.com/boostorg/log/blob/master/example/basic_usage/main.cpp
		TEST_METHOD(logging){

			std::thread t1(&Strat::unit_test::test_log, "thread1 ");
			std::thread t2(&Strat::unit_test::test_log, "thread2 ");

			t1.join();
			t2.join();
		}

#pragma endregion

		TEST_METHOD(event_long_short_process_tick_with_stoploss)
		{
			strat::event_long_short algo("eurusd", "../../test_files/Calendar-11-24-2013.csv"
				, 5, 15, 0.0003, true);

			std::vector<strat::tick> tick_vec;
			//util::read_tick_csv("../../test_files/EURUSD_min_11-24-2013.csv", tick_vec);

			strat::position close_pos;
			std::queue<strat::tick> obser_q;
			strat::signal sig = strat::signal::NONE;

			for (int i = 0; i < 10; i++){
				algo.process_tick(tick_vec[i], close_pos);
				obser_q = algo.get_obser_tick_queue();
				Assert::IsTrue(obser_q.empty());
				Assert::IsTrue(strat::signal::NONE == sig);
			}

			//index no here = 'row index in file' - 2 (exclude header and c++ is 0 start index)
			sig = algo.process_tick(tick_vec[298], close_pos);
			obser_q = algo.get_obser_tick_queue();
			Assert::IsTrue(obser_q.empty());
			Assert::IsTrue(close_pos.type == strat::signal::NONE);
			Assert::IsTrue(strat::signal::NONE == sig);

			sig = algo.process_tick(tick_vec[299], close_pos);
			obser_q = algo.get_obser_tick_queue();
			size_t size = 1;
			Assert::AreEqual(obser_q.size(), size);
			Assert::IsTrue(close_pos.type == strat::signal::NONE);
			Assert::IsTrue(strat::signal::NONE == sig);

			sig = algo.process_tick(tick_vec[304], close_pos);
			obser_q = algo.get_obser_tick_queue();
			Assert::IsTrue(close_pos.type == strat::signal::NONE);
			Assert::IsTrue(strat::signal::BUY == sig);
			Assert::IsTrue(algo.has_open_position());
			strat::position pos = algo.get_position();
			Assert::IsTrue(pos.open_tick.last == 1.3540);

			sig = algo.process_tick(tick_vec[317], close_pos, 0.0003);
			obser_q = algo.get_obser_tick_queue();
			Assert::IsTrue(obser_q.empty());
			Assert::IsTrue(strat::signal::NONE == sig);
			Assert::IsTrue(close_pos.type != strat::signal::NONE);
			Assert::IsTrue(!algo.has_open_position());
		}

		TEST_METHOD(sma){ 

			strat::sma ma(5);

			//mean of last 5 = 0.654106
			std::vector<double> t = { 0.1538291, 0.7083242, 0.9330954, 0.7555677, 0.1090589, 1.2543208, 0.7245816, 0.3999856, 0.4196101, 0.4720318 };
			for (std::vector<double>::iterator it = t.begin(); it != t.end(); ++it){
			
				ma.push(*it);
			}

			Assert::IsTrue(ma.get_value() - 0.654106 < 0.000001);
		}

		TEST_METHOD(trend){
		
			strat::trend trd(10);

			//slope = -0.004669211
			//std::vector<double> t = { 0.1538291, 0.7083242, 0.9330954, 0.7555677, 0.1090589, 1.2543208, 0.7245816, 0.3999856, 0.4196101, 0.4720318 };
			//y = 2x + 10 +- error(1)
			std::vector<double> t = { 11, 15, 15, 19, 19, 23, 23, 27, 27, 31 };
			double slope = 0;
			strat::trend_type type = trd.get_trend(t, slope);

			Assert::IsTrue(slope/1000000 - 2 < 0.1);
			Assert::IsTrue(type == strat::trend_type::UP);
		}

		TEST_METHOD(event_ma_process_tick){

			strat::event_algo_ma algo("eurusd", "../../test_files/Calendar-11-24-2013.csv",
				 5, 15, 0.0003, 15, 60);

			std::vector<strat::tick> tick_vec;
			//util::read_tick_csv("../../test_files/EURUSD_min_11-24-2013.csv", tick_vec);

			strat::position close_pos;
			std::queue<strat::tick> obser_q;
			strat::signal sig = strat::signal::NONE;

			for (int i = 0; i < 10; i++){
				algo.process_tick(tick_vec[i], close_pos);
				obser_q = algo.get_obser_tick_queue();
				Assert::IsTrue(obser_q.empty());
				Assert::IsTrue(strat::signal::NONE == sig);
			}

			size_t size = 1;
			for (int i = 100; i < 320; i++){
				//index no here = 'row index in file' - 2 (exclude header and c++ is 0 start index)
				sig = algo.process_tick(tick_vec[i], close_pos);

				if (i == 298){

					obser_q = algo.get_obser_tick_queue();
					Assert::IsTrue(obser_q.empty());
					Assert::IsTrue(close_pos.type == strat::signal::NONE);
					Assert::IsTrue(strat::signal::NONE == sig);
				}

				if (i == 299){

					obser_q = algo.get_obser_tick_queue();
					Assert::AreEqual(obser_q.size(), size);
					Assert::IsTrue(close_pos.type == strat::signal::NONE);
					Assert::IsTrue(strat::signal::NONE == sig);
				}

				if (i == 304){

					obser_q = algo.get_obser_tick_queue();
					Assert::IsTrue(close_pos.type == strat::signal::NONE);
					Assert::IsTrue(strat::signal::BUY == sig);
					Assert::IsTrue(algo.has_open_position());
					strat::position pos = algo.get_position();
					Assert::IsTrue(pos.open_tick.last == 1.3540);
				}

				if (i == 319){

					obser_q = algo.get_obser_tick_queue();
					Assert::IsTrue(obser_q.empty());
					Assert::IsTrue(strat::signal::NONE == sig);
					Assert::IsTrue(close_pos.type != strat::signal::NONE);
					Assert::IsTrue(!algo.has_open_position());
				}
			}
		}
		
#pragma region optimizer

		TEST_METHOD(algo_bollinger_optimize){
		
			//strat::algo_bollinger* algo_p = new strat::algo_bollinger("eurusd", 1, 16, 0.028, 0.092);
			//strat::optimizer_genetic<size_t, size_t, double, double> optimizer(
			//	"C:/workspace/Strat/test_files/USDJPY2013.csv", algo_p, 0.20f, 0.40f, 1, 1);

			//boost::posix_time::ptime start =
			//	boost::posix_time::time_from_string(std::string("2013-01-01 00:00:00.000"));
			//boost::posix_time::ptime end =
			//	boost::posix_time::time_from_string(std::string("2013-02-01 00:00:00.000"));
			//std::pair<double, std::tuple<size_t, size_t, double, double>> opti_params = optimizer.optimize(
			//	start, end);

			//delete(algo_p);

			throw("wrong");
		}

#pragma endregion optimizer

		TEST_METHOD(write_read_ini){
		
			int i1 = rand(), i2 = rand();
			util::write_ini("c:/strat_ini/strat_test.ini", "TEST.TEST1", i1);
			util::write_ini("c:/strat_ini/strat_test.ini", "TEST.TEST2", i2);

			Assert::AreEqual(util::read_ini<int>("c:/strat_ini/strat_test.ini", "TEST.TEST1"), i1);
			Assert::AreEqual(util::read_ini<int>("c:/strat_ini/strat_test.ini", "TEST.TEST2"), i2);
		}

		TEST_METHOD(algo_bollinger_process_tick){

			strat::algo_bollinger algo("eurusd", 1, 9, 0.0003, 0.0009);

			std::vector<strat::tick> tick_vec;

			std::vector<int> cols_v{ 0, 1, 2, 3 };
			auto start_date = util::convert_to_dt("20130108", "%Y%m%d");
			auto end_date = util::convert_to_dt("20130110", "%Y%m%d");
			util::read_tick_csv("../../test_files/EURUSD_2013_1min_alpari-Jan.csv", 
				tick_vec, start_date, end_date, "%Y.%m.%d %H:%M", cols_v);

			strat::position close_pos;
			strat::signal sig = strat::signal::NONE;

			size_t size = 1;
			for (int i = 0; i < 45; i++){

				sig = algo.process_tick(tick_vec[i], close_pos);

				if (i == 5){

					Assert::IsTrue(strat::signal::NONE == sig);
					Assert::IsTrue(!algo.has_open_position());
				}

				if (i == 9){

					Assert::IsTrue(strat::signal::SELL == sig);
					Assert::IsTrue(algo.has_open_position());
				}

				if (i == 45){

					Assert::IsTrue(close_pos.type == strat::signal::SELL);
					Assert::IsTrue(strat::signal::NONE == sig);
					Assert::IsTrue(!algo.has_open_position());
				}

				if (i == 46){

					Assert::IsTrue(strat::signal::NONE == sig);
					Assert::IsTrue(!algo.has_open_position());
				}

				if (i == 47){

					Assert::IsTrue(strat::signal::NONE == sig);
					Assert::IsTrue(!algo.has_open_position());
				}

				if (i == 49){

					Assert::IsTrue(strat::signal::NONE == sig);
					Assert::IsTrue(!algo.has_open_position());
				}

				if (i == 53){

					Assert::IsTrue(strat::signal::SELL == sig);
					Assert::IsTrue(algo.has_open_position());
				}

				if (i == 89){

					Assert::IsTrue(close_pos.type == strat::signal::SELL);
					Assert::IsTrue(strat::signal::NONE == sig);
					Assert::IsTrue(!algo.has_open_position());
				}
			}
		}

		TEST_METHOD(algo_dayrange_process_tick){

			strat::algo_dayrange algo("eurusd", 13, 0, 0.0005);

			std::vector<strat::tick> tick_vec;

			std::vector<int> cols_v{ 0, 1, 2, 3 };
			auto start_date = util::convert_to_dt("20130108", "%Y%m%d");
			auto end_date = util::convert_to_dt("20130110", "%Y%m%d");
			util::read_tick_csv("../../test_files/EURUSD_2013_1min_alpari-Jan.csv",
				tick_vec, start_date, end_date, "%Y.%m.%d %H:%M", cols_v);

			strat::position close_pos;
			strat::signal sig = strat::signal::NONE;

			size_t size = 1;
			for (int i = 0; i < 45; i++){

				sig = algo.process_tick(tick_vec[i], close_pos);

				if (i == 5){

					Assert::IsTrue(strat::signal::NONE == sig);
					Assert::IsTrue(!algo.has_open_position());
				}

				if (i == 3083){

					Assert::IsTrue(strat::signal::SELL == sig);
					Assert::IsTrue(algo.has_open_position());
				}

				if (i == 3084){

					Assert::IsTrue(close_pos.type == strat::signal::SELL);
					Assert::IsTrue(strat::signal::NONE == sig);
					Assert::IsTrue(!algo.has_open_position());
				}

				if (i == 3094){

					Assert::IsTrue(strat::signal::BUY == sig);
					Assert::IsTrue(algo.has_open_position());
				}

				if (i == 3095){

					Assert::IsTrue(close_pos.type == strat::signal::BUY);
					Assert::IsTrue(strat::signal::NONE == sig);
					Assert::IsTrue(!algo.has_open_position());
				}
			}
		}
	};
}

#endif