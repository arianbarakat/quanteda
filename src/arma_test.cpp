#include <RcppArmadillo.h>
#include <Rcpp.h>
using namespace Rcpp;

// [[Rcpp::export]]
double test_arma(arma::sp_mat x) {
    double s;
    for (int i = 0; i < x.n_cols; i++) {
        s += x.row(1)[i];
    }
    return(s);
}

// [[Rcpp::export]]
double test_arma2(arma::sp_mat x) {
    double s;
    for (int i = 0; i < x.n_cols; i++) {
        s += x.row(1)(i);
    }
    return(s);
}

// [[Rcpp::export]]
void test_arma3(arma::sp_mat x) {
    arma::rowvec s(x.n_cols);
    for (int i = 0; i < x.n_cols; i++) {
        s = arma::rowvec(x.col(i));
    }
}


// [[Rcpp::export]]
void test_arma4(arma::sp_mat x) {
    double t = 0;
    arma::sp_mat s = arma::sp_mat(x.n_rows, 1);
    for (int i = 0; i < x.n_cols; i++) {
        s = x.col(i);
    }
    for (int j = 0; j < s.n_rows; j++) {
        t += s[j];
    }
}

// [[Rcpp::export]]
void test_arma5(arma::sp_mat x) {
    double t = 0;
    arma::sp_mat s = arma::sp_mat(x.n_rows, 1);
    for (int i = 0; i < x.n_cols; i++) {
        s = x[i];
    }
    for (int j = 0; j < s.n_rows; j++) {
        t += s[j];
    }
}

// [[Rcpp::export]]
void test_arma10(arma::sp_mat x) {
    Rcout << x.at(1) << "\n";
    //Rcout << x.col(1) << "\n";
}


// [[Rcpp::export]]
double test_std(arma::sp_mat x) {
    std::vector<double> v(x.n_cols);
    double s;
    for (int i = 0; i < x.n_cols; i++) {
        s += x[i];
    }
    return(s);
}


// You can include R code blocks in C++ files processed with sourceCpp
// (useful for testing and development). The R code will be automatically 
// run after the compilation.
//

/*** R
mt = abs(Matrix::rsparsematrix(10000, 1000, density=0.02))
# microbenchmark::microbenchmark(
#     test_std(mt),
#     test_arma(mt),
#     test_arma2(mt)
# )

# microbenchmark::microbenchmark(
#     test_arma4(mt),
#     test_arma5(mt)
# )

test_arma10(mt)

*/
