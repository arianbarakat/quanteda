#include <Rcpp.h>
#include <RcppParallel.h>
#include <unordered_map>
#include <unordered_set>
#include <limits>
#include <algorithm>

// [[Rcpp::plugins(cpp11)]]
using namespace Rcpp;
// [[Rcpp::depends(RcppParallel)]]
using namespace RcppParallel;
using namespace std;

#ifndef QUANTEDA // prevent redefining
#define QUANTEDA

#define CLANG_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)

// setting for unordered_map and unordered_set 
const float GLOBAL_PATTERNS_MAX_LOAD_FACTOR = 0.1;
const float GLOBAL_NGRAMS_MAX_LOAD_FACTOR = 0.5;

// compiler has to be newer than clang 3.30 or gcc 4.8.1
#if RCPP_PARALLEL_USE_TBB && (CLANG_VERSION >= 30300 || GCC_VERSION >= 40801) 
#define QUANTEDA_USE_TBB true // tbb.h is loaded automatically by RcppParallel.h
#else
#define QUANTEDA_USE_TBB false
#endif

namespace quanteda{
    
    typedef ListOf<IntegerVector> Tokens;
    typedef std::vector<unsigned int> Text;
    typedef std::vector<Text> Texts;
    
#if QUANTEDA_USE_TBB
    typedef tbb::atomic<int> IntParam;
    typedef tbb::atomic<unsigned int> UintParam;
    typedef tbb::atomic<long> LongParam;
    typedef tbb::atomic<double> DoubleParam;
    typedef tbb::concurrent_vector<int> IntParams;
    typedef tbb::concurrent_vector<long> LongParams;
    typedef tbb::concurrent_vector<double> DoubleParams;
    typedef tbb::spin_mutex Mutex;
#else
    typedef int IntParam;
    typedef unsigned int UintParam;
    typedef long LongParam;
    typedef double DoubleParam;
    typedef std::vector<int> IntParams;
    typedef std::vector<long> LongParams;
    typedef std::vector<double> DoubleParams;
#endif    
    

    inline String join_strings(CharacterVector &tokens_, 
                       const String delim_ = " "){
        
        if (tokens_.size() == 0) return "";
        String token_ = tokens_[0];
        for (unsigned int i = 1; i < (unsigned int)tokens_.size(); i++) {
          token_ += delim_;
          token_ += tokens_[i];
        }
        token_.set_encoding(CE_UTF8);
        return token_;
    }
    
    inline std::string join_strings(std::vector<std::string> &tokens, 
                            const std::string delim = " "){
        if (tokens.size() == 0) return "";
        std::string token = tokens[0];
        for (std::size_t i = 1; i < tokens.size(); i++) {
          token += delim + tokens[i];
        }
        return token;
    }
    
    inline String join_strings(std::vector<unsigned int> &tokens, 
                       CharacterVector types_, 
                       const String delim_ = " ") {
        
        String token_("");
        if (tokens.size() > 0) {
            if (tokens[0] != 0) {
                token_ += types_[tokens[0] - 1];
            }
            for (std::size_t j = 1; j < tokens.size(); j++) {
                if (tokens[j] != 0) {
                    token_ += delim_;
                    token_ += types_[tokens[j] - 1];
                }
            }
            token_.set_encoding(CE_UTF8);
        }
        return token_;
    }
    
    inline bool has_na(IntegerVector vec_) {
        for (unsigned int i = 0; i < (unsigned int)vec_.size(); ++i) {
            if (vec_[i] == NA_INTEGER) return true;
        }
        return false;
    }
    
    /* 
     * This function is faster than Rcpp::wrap() but the stability need to be evaluated.
     */
    inline List as_list(Texts &texts, bool sort = false){
        List list(texts.size());
        for (std::size_t h = 0; h < texts.size(); h++) {
            Text text = texts[h];
            IntegerVector temp = Rcpp::wrap(text);
            list[h] = temp;
        }
        return list;
    }

    // Ngram functions and objects -------------------------------------------------------
    
    typedef std::vector<unsigned int> Ngram;
    typedef std::vector<Ngram> Ngrams;
    typedef std::string Type;
    typedef std::vector<Type> Types;
    
    struct hash_ngram {
            std::size_t operator() (const Ngram &vec) const {
            unsigned int add = 0;
            unsigned int seed = 0;
            for (std::size_t i = 0; i < vec.size(); i++) {
                add = vec[i] << (8 * i);
                if (seed <= UINT_MAX - add) { // check if addition will overflow seed
                    seed += add;
                } else {
                    return std::hash<unsigned int>()(seed);
                }
            }
            return std::hash<unsigned int>()(seed);
        }
    };
    
    /*
    struct hash_ngram {
        std::size_t operator() (const Ngram &vec) const {
            unsigned int hash = 0;
            hash ^= std::hash<unsigned int>()(vec[1]) + 0x9e3779b9;
            for (std::size_t i = 1; i < vec.size(); i++) {
                hash ^= std::hash<unsigned int>()(vec[i]) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        }
    };
    */
    struct equal_ngram {
        bool operator() (const Ngram &vec1, const Ngram &vec2) const { 
            return (vec1 == vec2);
        }
    };

#if QUANTEDA_USE_TBB
    typedef tbb::atomic<unsigned int> IdNgram;
    typedef tbb::concurrent_unordered_multimap<Ngram, UintParam, hash_ngram, equal_ngram> MultiMapNgrams;
    typedef tbb::concurrent_unordered_map<Ngram, UintParam, hash_ngram, equal_ngram> MapNgrams;
    typedef tbb::concurrent_unordered_set<Ngram, hash_ngram, equal_ngram> SetNgrams;
    typedef tbb::concurrent_vector<Ngram> VecNgrams;
    typedef tbb::concurrent_unordered_set<unsigned int> SetUnigrams;
#else
    typedef unsigned int IdNgram;
    typedef std::unordered_multimap<Ngram, unsigned int, hash_ngram, equal_ngram> MultiMapNgrams;
    typedef std::unordered_map<Ngram, unsigned int, hash_ngram, equal_ngram> MapNgrams;
    typedef std::unordered_set<Ngram, hash_ngram, equal_ngram> SetNgrams;
    typedef std::vector<Ngram> VecNgrams;
    typedef std::unordered_set<unsigned int> SetUnigrams;
#endif    
/*
    inline std::vector<std::size_t> register_ngrams(List words_, SetNgrams &set_words) {
        std::vector<std::size_t> spans(words_.size());
        for (unsigned int g = 0; g < (unsigned int)words_.size(); g++) {
            if (has_na(words_[g])) continue;
            Ngram word = words_[g];
            set_words.insert(word);
            spans[g] = word.size();
        }
        sort(spans.begin(), spans.end());
        spans.erase(unique(spans.begin(), spans.end()), spans.end());
        std::reverse(std::begin(spans), std::end(spans));
        return spans;
    }

    inline std::vector<std::size_t> register_ngrams(List words_, IntegerVector ids_, MapNgrams &map_words) {
        std::vector<std::size_t> spans(words_.size());
        for (unsigned int g = 0; g < (unsigned int)words_.size(); g++) {
            if (has_na(words_[g])) continue;
            Ngram word = words_[g];
            map_words[word] = ids_[g];
            spans[g] = word.size();
        }
        sort(spans.begin(), spans.end());
        spans.erase(unique(spans.begin(), spans.end()), spans.end());
        std::reverse(std::begin(spans), std::end(spans));
        return spans;
    }
*/
    inline std::vector<std::size_t> register_ngrams(List patterns_, SetNgrams &set) {

        set.max_load_factor(GLOBAL_PATTERNS_MAX_LOAD_FACTOR);
        Ngrams patterns = Rcpp::as<Ngrams>(patterns_);
        std::vector<std::size_t> spans(patterns.size());
        for (size_t g = 0; g < patterns.size(); g++) {
            set.insert(patterns[g]);
            spans[g] = patterns[g].size();
        }
        sort(spans.begin(), spans.end());
        spans.erase(unique(spans.begin(), spans.end()), spans.end());
        std::reverse(std::begin(spans), std::end(spans));
        return spans;
    }

    inline std::vector<std::size_t> register_ngrams(List patterns_, IntegerVector ids_, MapNgrams &map) {

        map.max_load_factor(GLOBAL_PATTERNS_MAX_LOAD_FACTOR);
        Ngrams patterns = Rcpp::as<Ngrams>(patterns_);
        std::vector<unsigned int> ids = Rcpp::as< std::vector<unsigned int> >(ids_);
        std::vector<std::size_t> spans(patterns.size());
        for (size_t g = 0; g < std::min(patterns.size(), ids.size()); g++) {
            map.insert(std::pair<Ngram, IdNgram>(patterns[g], ids[g]));
            spans[g] = patterns[g].size();
        }

        // Rcout << "current max_load_factor: " << map.max_load_factor() << std::endl;
        // Rcout << "current size           : " << map.size() << std::endl;
        // Rcout << "current bucket_count   : " << map.unsafe_bucket_count() << std::endl;
        // Rcout << "current load_factor    : " << map.load_factor() << std::endl;

        sort(spans.begin(), spans.end());
        spans.erase(unique(spans.begin(), spans.end()), spans.end());
        std::reverse(std::begin(spans), std::end(spans));
        return spans;
    }
    
// These typedefs are used in fcm_mt, ca, wordfish_mt
#if QUANTEDA_USE_TBB
    typedef std::tuple<unsigned int, unsigned int, double> Triplet;
    typedef tbb::concurrent_vector<Triplet> Triplets;
#else
    typedef std::tuple<unsigned int, unsigned int, double> Triplet;
    typedef std::vector<Triplet> Triplets;
#endif
}

#endif
