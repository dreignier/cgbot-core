if [ "$1" == "debug" ]
then
	g++ cgbot.cpp -o cgbot -std=c++17 -fsanitize=address -g -rdynamic -Wall
else
	g++ cgbot.cpp -o cgbot -std=c++17 -O3 -Wall
fi
