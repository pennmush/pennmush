// utils.h
//
// misc. helper functions

#pragma once

std::vector<std::string> split_on(const std::string &, char);
strset split_words(const std::string &);
std::string join_words(const strset &);
std::string get_time();
