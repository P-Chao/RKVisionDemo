#pragma once

#define LOGV(x) std::cout << "V [" << __FILE__ << "+L" << __LINE__ << "] " << x << std::endl
#define LOGI(x) std::cout << "I [" << __FILE__ << "+L" << __LINE__ << "] " << x << std::endl
#define LOGE(x) std::cout << "E*[" << __FILE__ << "+L" << __LINE__ << "] " << x << std::endl
#define LOGD(x) ;

uint32_t get_time_t0();
uint32_t get_time_t1(uint32_t t0);

