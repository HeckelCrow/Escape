#pragma once
#include <boost/unordered/unordered_flat_map.hpp>

template<typename Key, typename Value>
using Hashtable = boost::unordered_flat_map<Key, Value>;
