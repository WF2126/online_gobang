gobang:gobang.cpp util.hpp online.hpp db.hpp session.hpp room.hpp matcher.hpp gobang.hpp
	g++ -g -std=c++11 $^ -o $@ -L/usr/lib64/mysql -lmysqlclient -ljsoncpp -lboost_system -lpthread
clean:
	rm -rf gobang