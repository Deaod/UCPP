#include <vector>

struct decoder {
    std::vector<char32_t> decode(std::vector<std::byte> data);
};

std::vector<char32_t> decoder::decode(std::vector<std::byte> data) {
    


    //{ // try basic encoding
    //    auto result = ztd::text::transcode(input, ztd::text::utf8);
    //    if (result.errors_were_handled() == false)
    //        return result.output;
    //}
    //{
    //    auto result = ztd::text::transcode_to<std::vector<char32_t>>(input, ztd::text::utf8, ztd::text::utf32, ztd::text::pass_handler);
    //    if (result.errors_were_handled() == false)
    //        return result.output;
    //}
    //{
    //    auto result = ztd::text::transcode_to<std::vector<char32_t>>(input, ztd::text::utf16, ztd::text::utf32, ztd::text::pass_handler);
    //    if (result.errors_were_handled() == false)
    //        return result.output;
    //}
}

