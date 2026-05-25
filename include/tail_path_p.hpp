#pragma once

#include <restinio/all.hpp>
#include <restinio/router/easy_parser_router.hpp>
#include <string>

namespace epr = restinio::router::easy_parser_router;
namespace ep = restinio::easy_parser;

namespace origo {
    // Продюсер читает оставшийся путь до конца
    inline auto tail_path_p()
    {
        // ep::N — это специальная константа RESTinio, означающая "бесконечное количество повторений"
        return ep::produce<std::string>(
            ep::repeat(
                0,          // Минимальное количество символов (поменяйте на 0, если хвост может быть пустым)
                ep::N,      // Читать до самого конца строки (жадный поиск)
                ep::any_symbol_p() >> ep::to_container() // to_container() автоматически пушит char в std::string
            )
        );
    }
}