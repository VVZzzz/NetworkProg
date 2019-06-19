#include <iostream>
#include <vector>

void deleteArrIdx2(std::vector<int> &dataArr, std::vector<int> &idxArr) {
  //空间复杂度O(1),时间复杂度O(n)
  if (idxArr.empty()) return;
  int newSize = dataArr.size() - idxArr.size();
  idxArr.push_back(dataArr.size());
  int exchangePoint = idxArr[0];
  for (int i = 0; i < idxArr.size() - 1; i++) {
    int j = idxArr[i] + 1;
    for (; j < idxArr[i + 1]; j++) {
      dataArr[exchangePoint++] = dataArr[j];
    }
  }
  dataArr.resize(newSize);
}

int main() {
  std::vector<int> dataArr = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  std::vector<int> idxArr = {2, 4, 6, 7, 8};
  deleteArrIdx2(dataArr, idxArr);
  for (int i : dataArr) std::cout << i << " ";
  std::cout << std::endl;
  return 0;
}