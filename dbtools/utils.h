// utils.h
//
// misc. helper functions

#pragma once

stringvec split_on(string_view, char);
stringset split_words(string_view);
stringvec split_words_vec(string_view);
std::string join_words(const stringset &);
std::string join_words(const stringvec &);
std::string get_time();
