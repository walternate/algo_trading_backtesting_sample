
#ifndef _UTIL
#define _UTIL

#include "tick.h"

#include<vector>
#include<list>
#include<string>
#include<fstream>
#include<sstream>

#include <boost/tokenizer.hpp>
#include <boost/date_time.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

using std::string;

class util{
public:
	static int read_csv(string path, std::vector<std::vector<std::string>>& vec, const std::vector<int>& cols, bool with_header = true){
		
		try {

			std::ifstream file(path);
			std::string line;
			size_t cols_len = cols.size();

			if (with_header) std::getline(file, line);

			while (std::getline(file, line)) {
				
				boost::tokenizer<boost::escaped_list_separator<char> > tk(
					line, boost::escaped_list_separator<char>('\\', ',', '\"'));
				int cols_i = 0;
				int i = 0;
				std::vector<std::string> row_vec;

				for (boost::tokenizer<boost::escaped_list_separator<char>>::iterator it(tk.begin());
					it != tk.end(); ++it)
				{
					if (cols_i >= cols_len) break;

					if (cols[cols_i] == i++){
						row_vec.push_back(*it);
						cols_i++;
					}
				}

				vec.push_back(row_vec);
			}

			return 1;
		}
		catch (std::ifstream::failure e) {
			//std::cerr << "Exception opening/reading/closing file\n";
			throw;
		}
		catch (...) { 
			throw;
		}
	}

	static boost::posix_time::ptime convert_to_dt(const std::string s, const std::string format = "%Y %a %b %d %H:%M")	{

		const std::locale f = std::locale(std::locale::classic(), new  boost::posix_time::time_input_facet(format));

		boost::posix_time::ptime pt;
		std::istringstream is(s);
		is.imbue(f);
		is >> pt;

		return pt;
	}

	static int read_tick_csv(string path, std::vector<strat::tick>& tick_vec, 
		string dt_format = "%Y%m%d%H%M%S", const std::vector<int>& cols_v = { 1, 2, 6 }){
		
		std::vector<std::vector<std::string>> csv_vec;
		util::read_csv(path, csv_vec, cols_v);

		boost::posix_time::ptime t;
		for (std::vector<std::vector<std::string>>::iterator it = csv_vec.begin(); it != csv_vec.end(); ++it){

			if (cols_v.size() >= 3) // combine first two cols for date time
				t = util::convert_to_dt((*it)[0] + (*it)[1], dt_format);
			else // first single col for date time
				t = util::convert_to_dt((*it)[0] + (*it)[1], dt_format);
			strat::tick tick1;
			tick1.time_stamp = t;
			tick1.close = boost::lexical_cast<double>((*it)[2]);
			tick_vec.push_back(tick1);
		}

		return 1;
	}

	static string get_current_dt_str(string format = "%Y%m%d.%H%M%S"){
	
		std::stringstream s;

		const boost::posix_time::ptime now =
			boost::posix_time::second_clock::local_time();

		boost::posix_time::time_facet*const f =
			new boost::posix_time::time_facet("%Y%m%d.%H%M%S");
		s.imbue(std::locale(s.getloc(), f));
		s << now;

		return s.str();
	}

	static string dt_to_string(boost::posix_time::ptime pt){

		return boost::posix_time::to_simple_string(pt);
	}

	template<typename T>
	static T read_ini(string path, string section){
	
		boost::property_tree::ptree pt;
		boost::property_tree::ini_parser::read_ini(path, pt);
		return pt.get<T>(section);
	}

	template<typename T>
	static void write_ini(string path, string section, T value){

		boost::property_tree::ptree pt;
		boost::property_tree::ini_parser::read_ini(path, pt);
		pt.put(section, value);
	}

	static int delete_hist_tick(string path, int keep_days_no){

		try {

			std::ifstream file(path);
			std::string line;
			std::string tmp_f_path = path + ".tmp";
			std::ofstream tmp_file(tmp_f_path);

			//header
			std::getline(file, line);
			tmp_file << line;
			
			boost::posix_time::ptime t_no_early_than = 
				boost::posix_time::second_clock::local_time() - boost::posix_time::hours(keep_days_no * 24);

			while (std::getline(file, line)) {

				boost::tokenizer<boost::escaped_list_separator<char> > tk(
					line, boost::escaped_list_separator<char>('\\', ',', '\"'));

				boost::posix_time::ptime t = util::convert_to_dt(*tk.begin(), "%Y%m%d %H%M");
				if (t.date() >= t_no_early_than.date())
					continue;

				tmp_file << line;
			}

			file.close();
			tmp_file.close();

			if (0 == remove(path.c_str())){
			
				rename(tmp_f_path.c_str(), path.c_str());
			}

			return 1;
		}
		catch (std::ifstream::failure e) {
			//std::cerr << "Exception opening/reading/closing file\n";
			throw;
		}
		catch (...) {
			throw;
		}
	}

};

#endif