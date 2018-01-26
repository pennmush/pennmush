// utils.h
//
// misc. helper functions

#pragma once

std::vector<std::string> split_on(const std::string &, char);
stringset split_words(const std::string &);
std::string join_words(const stringset &);
std::string get_time();
