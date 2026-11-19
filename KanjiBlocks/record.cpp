/**
 * @file record.cpp
 * @brief 自己ベストの実装です。
 *
 * ファイルには、自己ベストの数を1つだけ書きます。
 *
 * このファイルは AI(Claude)が書いたものです。
 */
#include "record.h"

#include <fstream>

namespace {

/** 自己ベストをファイルから読みます。ファイルが無いときや読めないときは 0 です。 */
int LoadBestScore() {
    std::ifstream file(kBestScoreFile);
    int score = 0;
    if (!(file >> score) || score < 0) {
        return 0;
    }
    return score;
}

/** 今の自己ベストです。最初に使うときにファイルから読みます。 */
int& BestScore() {
    static int best_score = LoadBestScore();
    return best_score;
}

}  // namespace

int GetBestScore() {
    return BestScore();
}

bool UpdateBestScore(int score) {
    if (score <= BestScore()) {
        return false;
    }

    BestScore() = score;

    std::ofstream file(kBestScoreFile);
    file << score << '\n';
    return true;
}
