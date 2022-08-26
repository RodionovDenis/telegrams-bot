#include "push-up-bot.h"
#include "reader-bot.h"
#include "clock.h"

int main() {
    //PushUpBot().Run();
    //ReaderBot().Run();
    std::cout << CurrentTime().DiffIntervals(1661529518, 1660319939) << std::endl;
}