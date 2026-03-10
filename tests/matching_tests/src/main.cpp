#include <gtest/gtest.h>

// Функция, которую мы хотим протестировать
int Sum(int a, int b) {
    return a + b;
}

// Макрос TEST(НазваниеГруппы, НазваниеТеста)
TEST(MathTests, AdditionTrue) {
    // EXPECT_EQ проверяет равенство. Если не равно — тест упадет, 
    // но выполнение функции продолжится.
    EXPECT_EQ(Sum(2, 2), 4);
}

TEST(MathTests, AdditionFalse) {
    // Специально провалим тест для примера
    EXPECT_EQ(Sum(1, 1), 3); 
}