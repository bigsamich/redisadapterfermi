/**
 * RedisAdapterSingle.C
 *
 * This file contains the implementation of the RedisAdapterSingle class.
 *
 * @author rsantucc
 */


#include "RedisAdapterSingle.hpp"

using namespace sw::redis;
using namespace std;

RedisAdapterSingle::RedisAdapterSingle( string key, string connection )
:_redis(connection),
 _baseKey(key)
{
  _connection = connection;
  _configKey  = _baseKey + ":CONFIG";
  _logKey     = _baseKey + ":LOG";
  _channelKey = _baseKey + ":CHANNEL";
  _statusKey  = _baseKey + ":STATUS";
  _timeKey    = _baseKey + ":TIME";
  _dataBaseKey= _baseKey + ":DATA";
  _deviceKey = _baseKey + ":DEVICES";

  TRACE(2,"Loaded Redis Adapters");

}

RedisAdapterSingle::RedisAdapterSingle(const RedisAdapterSingle& ra)
:_redis(ra._connection)
{
  _baseKey    = ra.getBaseKey();
  _configKey  = _baseKey + ":CONFIG";
  _logKey     = _baseKey + ":LOG";
  _channelKey = _baseKey + ":CHANNEL";
  _statusKey  = _baseKey + ":STATUS";
  _timeKey  = _baseKey + ":TIME";
  _dataBaseKey  = _baseKey + ":DATA";
  _deviceKey = _baseKey + ":DEVICES";
}


RedisAdapterSingle::~RedisAdapterSingle()
{
}


vector<string> RedisAdapterSingle::getDevices(){

  std::unordered_set<string> set = getSet(_deviceKey);
  std::vector<string> devices(set.begin(), set.end());
  return devices;

}

void RedisAdapterSingle::clearDevices(string devicelist)
{
  std::unordered_map<string, string> nameMap;
  _redis.hgetall(devicelist, inserter(nameMap, nameMap.begin()));
  for(const auto& name : nameMap){
    _redis.del(name.first);
  }
}
/*
* Get Device Config
*/
void RedisAdapterSingle::setDeviceConfig(std::unordered_map<std::string, std::string> map){

    setHash(_configKey, map);

}

std::unordered_map<std::string, std::string> RedisAdapterSingle::getDeviceConfig(){
    return getHash(_configKey);
}

void RedisAdapterSingle::setDevice(string name){
  setSet(_deviceKey, name);
}



 string RedisAdapterSingle::getValue(string key){

  return *(_redis.get(key));
}

void RedisAdapterSingle::setValue(string key, string val){
  _redis.set(key,val);
}


int RedisAdapterSingle::getUniqueValue(string key){
  return _redis.incr(key);
} 

/*
* Hash get and set
*/
unordered_map<string, string> RedisAdapterSingle::getHash(string key){

  std::unordered_map<std::string, std::string> m;
  _redis.hgetall(key, std::inserter(m, m.begin()));

  return m;
}

void RedisAdapterSingle::setHash(string key, std::unordered_map<std::string, std::string> m){

   return _redis.hmset(key, m.begin(), m.end());
}


/*
* Set get and add member
*/
std::unordered_set<string> RedisAdapterSingle::getSet(string key){

  std::unordered_set<string> set;
  try{
    _redis.smembers( key, std::inserter(set, set.begin()));

  }
  catch(...){

  }
  return set;
}

void RedisAdapterSingle::setSet(string key, string val){
  _redis.sadd(key,val);
}



/*
* Stream Functions
*/
void RedisAdapterSingle::streamWrite(vector<pair<string,string>> data, string timeID , string key, uint trim ){
  try{
    auto replies  =  _redis.xadd(key, timeID, data.begin(), data.end());  
    if(trim)
      streamTrim(key, trim);
  }catch (const std::exception &err) {
    TRACE(1,"xadd(" + key + ", " + ", ...) failed: " + err.what());
  }
}


string RedisAdapterSingle::streamReadBlock(std::unordered_map<string,string> keysID, int count, std::unordered_map<string,vector<float>>& dest){
  string timeID = "";
  try{
    std::unordered_map<std::string, ItemStream> result;
    _redis.xread(keysID.begin(),keysID.end(), count, std::inserter(result, result.end()) );
    for(auto key : result){
      string dataKey = key.first;
      for(auto data : key.second){
        timeID = data.first;
        for (auto val : data.second){
          dest[dataKey].resize(val.second.length() / sizeof(float));
          memcpy(dest[dataKey].data(),val.second.data(),val.second.length());
        }
      }
    }
    return timeID;  
  }catch (const std::exception &err) {
    TRACE(1,"streamReadBlock fail time: " +timeID+" err: " + err.what());
    return "$";
  }
}

void RedisAdapterSingle::streamRead(string key, string time, int count, ItemStream& dest){

  try{
    _redis.xrevrange(key, "+","-", count, back_inserter(dest));
  }catch (const std::exception &err) {
    TRACE(1,"xadd(" + key + ", " +time + ":" + to_string(count) + ", ...) failed: " + err.what());
  }
}


void RedisAdapterSingle::streamRead(string key, string time, int count, vector<float>& dest){

  try{
    ItemStream result;
    streamRead(key,time,1, result);
    for(auto data : result){
        string timeID = data.first;
        for (auto val : data.second){
            if(!val.first.compare("DATA")){
              dest.resize(val.second.length() / sizeof(float));
              memcpy(dest.data(),val.second.data(),val.second.length());
            }
        }
    }  
  }catch (const std::exception &err) {
    TRACE(1,"xadd(" + key + ", " +time + ":" + to_string(count) + ", ...) failed: " + err.what());
  }
}

void RedisAdapterSingle::logWrite(string key, string msg, string source){
  vector<pair<string,string>> data;
  data.emplace_back(make_pair(source,msg));
  streamWrite(data, "*", key, 1000);
}

IRedisAdapter::ItemStream RedisAdapterSingle::logRead(uint count){

  ItemStream is ;
  try{ 
    _redis.xrevrange(getLogKey(), "+","-", count, back_inserter(is));
  } catch (const std::exception &err) {
    TRACE(1,"logRead(" + getLogKey()  + "," + to_string(count) + ") failed: " + err.what());
  }
  return is;
}

void RedisAdapterSingle::streamTrim(string key, int size){
  try{
      _redis.xtrim(key, size, false);

  }catch (const std::exception &err) {
    TRACE(1,"xtrim(" + key + ", " +to_string(size)+ ", ...) failed: " + err.what());
  }
}



void RedisAdapterSingle::publish(string msg){
  try{
      _redis.publish(_channelKey, msg);

  }catch (const std::exception &err) {
    TRACE(1,"publish(" + _channelKey + ", " +msg+ ", ...) failed: " + err.what());
  }
}
void RedisAdapterSingle::publish(string key, string msg){
  try{
      _redis.publish(_channelKey + ":" + key, msg);

  }catch (const std::exception &err) {
    TRACE(1,"publish(" + _channelKey + ", " +msg+ ", ...) failed: " + err.what());
  }
}

inline bool const StringToBool(string const& s){
    return s != "0";
  }
bool RedisAdapterSingle::getDeviceStatus() {
  return StringToBool(getValue(_statusKey));
}
void RedisAdapterSingle::setDeviceStatus(bool status){
  setValue(getStatusKey(), to_string((int)status));
}


void RedisAdapterSingle::copyKey( string src, string dest, bool data){
  if (data)
    _redis.command<void>("copy", src, dest);
  else
    _redis.command<void>("copy", src, dest);

}

void RedisAdapterSingle::deleteKey( string key ){
  _redis.command<long long>("del", key);
}


//inline bool const StringToBool(string const& s){
//    return s != "0";
//  }
bool RedisAdapterSingle::getAbortFlag(){
  return StringToBool(getValue(_abortKey));
}

inline const char * const BoolToString(bool b){
    return b ? "1" : "0";
}
void RedisAdapterSingle::setAbortFlag(bool flag){
  setValue(_abortKey, BoolToString(flag));
}


vector<string> RedisAdapterSingle::getServerTime(){

  std::vector<string> result;
  _redis.command("time", std::back_inserter(result));
  return result;
}




void RedisAdapterSingle::psubscribe(std::string pattern, std::function<void(std::string,std::string,std::string)> func){
  patternSubscriptions.emplace(pattern, func);
}

void RedisAdapterSingle::subscribe(std::string channel, std::function<void(std::string,std::string)> func){
  subscriptions.emplace(channel, func);
}

void RedisAdapterSingle::startListener(){
  _listener = thread(&RedisAdapterSingle::listener, this);
}
void RedisAdapterSingle::startReader(){
  _reader = thread(&RedisAdapterSingle::reader, this);
}

void RedisAdapterSingle::registerCommand(std::string command, std::function<void(std::string, std::string)> func){
  commands.emplace(_channelKey +":"+ command, func);
}


void RedisAdapterSingle::listener(){
  // Consume messages in a loop.
  bool flag = false;
  Subscriber _sub = _redis.subscriber();
  while (true) {
    try {
        if(flag)
            _sub.consume();
        else{
            flag = true;

            _sub.on_pmessage([&](std::string pattern, std::string key, std::string msg) { 

                auto search = commands.find(key);;
                if(search != commands.end()){
                  search->second(key, msg);
                }
                else{
                  auto patternsearch = patternSubscriptions.find(pattern);
                  if (patternsearch != patternSubscriptions.end()) {
                    patternsearch->second(pattern, key, msg);
                  }
                }
            });

            _sub.on_message([&](std::string key, std::string msg) { 

                  auto search = subscriptions.find(msg);
                  if (search != subscriptions.end()) {
                    search->second( key, msg);
                  }
            });
            //The default is everything published on ChannelKey
            _sub.psubscribe(_channelKey + "*");

        }
    }
    catch(const TimeoutError &e) {
        continue;
    }
    catch (...) {
        // Handle unrecoverable exceptions. Need to re create redis connection
        std::cout << "AN ERROR OCCURED, trying to recover" << std::endl;
        flag = false;
        _sub = _redis.subscriber();
        continue;
    }
  }
}

void RedisAdapterSingle::reader(){
  // Read the stream for data

  while (true) {
    try {
        //streamtime = _redis.streamReadBlock(streamKeys, 1, buffer);

    }
    catch(const TimeoutError &e) {
        continue;
    }
    catch (...) {
        // Handle unrecoverable exceptions. Need to re create redis connection
        std::cout << "AN ERROR OCCURED, trying to recover" << std::endl;
        continue;
    }
  }
}




















