#include "skaptcha.h"
#include "skaptcha_backend/captcha.h"
#include "skaptcha_tools.h"
#include "token.h"

const char TOKENDELIMITER = '|';

Skaptcha &Skaptcha::instance()
{
    static Skaptcha sk;
    return sk;
}

unsigned int Skaptcha::answerLength()
{
    return CAPTCHA_STRING_LENGTH;
}

std::shared_ptr<Skaptcha::View> Skaptcha::generate(const std::string &context, std::chrono::seconds lifetime)
{
    auto viewPtr = std::make_shared<View>();

    char *img = nullptr;
    int imgsize = 0;
    std::string answer(CAPTCHA_STRING_LENGTH+1, 0);

    generate_captcha(&img, &imgsize, answer.data());
    answer.pop_back(); // termination zero

    auto& viewPng = viewPtr->png;
    viewPng.resize(imgsize);
    for (int i = 0; i < imgsize; ++i)
    {
        viewPng[i] = img[i];
    }

    delete[] img;

    auto token = skaptcha_tools::ExpiringToken::generate(answer + TOKENDELIMITER + skaptcha_tools::base64::encode(context), lifetime);

    viewPtr->token = token->dump();

    return viewPtr;
}

bool Skaptcha::validate(const std::string &context, const std::string &token, const std::string &answer)
{
    if (answer.length() != CAPTCHA_STRING_LENGTH)
    {
        return false;
    }

    auto sharedToken = skaptcha_tools::ExpiringToken::fromString(token);
    if (sharedToken == nullptr)
    {
        return false;
    }

    const auto vector = skaptcha_tools::string::split(sharedToken->payload(), TOKENDELIMITER);
    if (vector.size() != 2)
    {
        return false;
    }

    std::string contextFromToken;
    try {
        const auto digest = skaptcha_tools::base64::decode(vector[1]);
        contextFromToken = {digest.begin(), digest.end()};
    } catch (...) {
        return false;
    }

    if (contextFromToken != context)
    {
        return false;
    }

    std::string userAnswerUpper; // Captcha is uppercase
    for (const auto& ch: answer)
    {
        userAnswerUpper.push_back( std::toupper(ch) );
    }

    return vector[0] == userAnswerUpper;
}

Skaptcha::Skaptcha()
{
    srand(time(NULL));
}

