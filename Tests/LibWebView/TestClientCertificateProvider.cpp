/*
 * Copyright (c) 2026, Kevin Bortis
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <LibURL/Parser.h>
#include <LibURL/URL.h>
#include <LibWebView/CLIClientCertificateProvider.h>
#include <LibWebView/ClientCertificateChain.h>

using WebView::CLIClientCertificateProvider;
using WebView::ClientCertificateChain;
using WebView::ClientCertificateProvider;
using WebView::ClientCertificateResult;

static URL::URL must_parse_url(StringView input)
{
    return URL::Parser::basic_parse(input).release_value();
}

// -- Mock provider for chain tests ------------------------------------------

class TestCertificateProvider final : public ClientCertificateProvider {
public:
    TestCertificateProvider(StringView name, Function<ClientCertificateResult(URL::URL const&)> callback)
        : m_name(name)
        , m_callback(move(callback))
    {
    }

    ClientCertificateResult provide(URL::URL const& url) override
    {
        ++m_call_count;
        return m_callback(url);
    }

    StringView name() const override { return m_name; }
    int call_count() const { return m_call_count; }

private:
    StringView m_name;
    Function<ClientCertificateResult(URL::URL const&)> m_callback;
    int m_call_count { 0 };
};

static NonnullOwnPtr<TestCertificateProvider> make_certificate_provider(StringView name, ByteString cert, ByteString key)
{
    return make<TestCertificateProvider>(name, [cert = move(cert), key = move(key)](URL::URL const&) -> ClientCertificateResult {
        return { ClientCertificateResult::Outcome::Certificate, { cert, key } };
    });
}

static NonnullOwnPtr<TestCertificateProvider> make_no_match_provider(StringView name)
{
    return make<TestCertificateProvider>(name, [](URL::URL const&) -> ClientCertificateResult {
        return {};
    });
}

static NonnullOwnPtr<TestCertificateProvider> make_no_certificate_provider(StringView name)
{
    return make<TestCertificateProvider>(name, [](URL::URL const&) -> ClientCertificateResult {
        return { ClientCertificateResult::Outcome::NoCertificate, {} };
    });
}

// -- CLI provider tests -----------------------------------------------------

TEST_CASE(cli_provider_exact_host_match)
{
    auto provider = TRY_OR_FAIL(CLIClientCertificateProvider::create({
        { "example.com"sv, "CERT"sv, "KEY"sv },
    }));

    auto result = provider->provide(must_parse_url("https://example.com/"sv));
    EXPECT_EQ(result.outcome, ClientCertificateResult::Outcome::Certificate);
    EXPECT_EQ(result.certificate_and_key.certificate, "CERT"sv);
    EXPECT_EQ(result.certificate_and_key.key, "KEY"sv);
}

TEST_CASE(cli_provider_case_insensitive_match)
{
    auto provider = TRY_OR_FAIL(CLIClientCertificateProvider::create({
        { "example.com"sv, "CERT"sv, "KEY"sv },
    }));

    auto result = provider->provide(must_parse_url("https://EXAMPLE.COM/"sv));
    EXPECT_EQ(result.outcome, ClientCertificateResult::Outcome::Certificate);
}

TEST_CASE(cli_provider_no_match)
{
    auto provider = TRY_OR_FAIL(CLIClientCertificateProvider::create({
        { "example.com"sv, "CERT"sv, "KEY"sv },
    }));

    auto result = provider->provide(must_parse_url("https://other.com/"sv));
    EXPECT_EQ(result.outcome, ClientCertificateResult::Outcome::NoMatch);
}

TEST_CASE(cli_provider_wildcard_match)
{
    auto provider = TRY_OR_FAIL(CLIClientCertificateProvider::create({
        { "*"sv, "WILD_CERT"sv, "WILD_KEY"sv },
    }));

    auto result = provider->provide(must_parse_url("https://anything.example.org/"sv));
    EXPECT_EQ(result.outcome, ClientCertificateResult::Outcome::Certificate);
    EXPECT_EQ(result.certificate_and_key.certificate, "WILD_CERT"sv);
}

TEST_CASE(cli_provider_exact_before_wildcard)
{
    auto provider = TRY_OR_FAIL(CLIClientCertificateProvider::create({
        { "specific.com"sv, "SPECIFIC_CERT"sv, "SPECIFIC_KEY"sv },
        { "*"sv, "WILD_CERT"sv, "WILD_KEY"sv },
    }));

    auto exact = provider->provide(must_parse_url("https://specific.com/"sv));
    EXPECT_EQ(exact.outcome, ClientCertificateResult::Outcome::Certificate);
    EXPECT_EQ(exact.certificate_and_key.certificate, "SPECIFIC_CERT"sv);

    auto wildcard = provider->provide(must_parse_url("https://other.com/"sv));
    EXPECT_EQ(wildcard.outcome, ClientCertificateResult::Outcome::Certificate);
    EXPECT_EQ(wildcard.certificate_and_key.certificate, "WILD_CERT"sv);
}

// -- Chain: provider iteration tests ----------------------------------------

TEST_CASE(chain_empty_returns_empty)
{
    ClientCertificateChain chain;
    auto result = chain.query(must_parse_url("https://example.com/"sv));
    EXPECT(result.certificate.is_empty());
    EXPECT(result.key.is_empty());
}

TEST_CASE(chain_has_providers)
{
    ClientCertificateChain chain;
    EXPECT(!chain.has_providers());

    chain.add_provider(make_no_match_provider("test"sv));
    EXPECT(chain.has_providers());
}

TEST_CASE(chain_queries_providers_in_order)
{
    ClientCertificateChain chain;
    chain.add_provider(make_no_match_provider("first"sv));
    chain.add_provider(make_certificate_provider("second"sv, "CERT2"sv, "KEY2"sv));

    auto result = chain.query(must_parse_url("https://example.com/"sv));
    EXPECT_EQ(result.certificate, "CERT2"sv);
    EXPECT_EQ(result.key, "KEY2"sv);
}

TEST_CASE(chain_stops_on_no_certificate)
{
    ClientCertificateChain chain;
    chain.add_provider(make_no_certificate_provider("blocker"sv));
    chain.add_provider(make_certificate_provider("unreachable"sv, "CERT"sv, "KEY"sv));

    auto result = chain.query(must_parse_url("https://example.com/"sv));
    EXPECT(result.certificate.is_empty());
}

// -- Chain: cache tests -----------------------------------------------------

TEST_CASE(chain_caches_positive_result)
{
    auto* raw_provider = new TestCertificateProvider("counting"sv,
        [](URL::URL const&) -> ClientCertificateResult {
            return { ClientCertificateResult::Outcome::Certificate, { "CERT"sv, "KEY"sv } };
        });

    ClientCertificateChain chain;
    chain.add_provider(NonnullOwnPtr<TestCertificateProvider>(adopt_own(*raw_provider)));

    chain.query(must_parse_url("https://example.com/"sv));
    chain.query(must_parse_url("https://example.com/"sv));

    EXPECT_EQ(raw_provider->call_count(), 1);
}

TEST_CASE(chain_caches_negative_result)
{
    auto* raw_provider = new TestCertificateProvider("counting"sv,
        [](URL::URL const&) -> ClientCertificateResult {
            return {};
        });

    ClientCertificateChain chain;
    chain.add_provider(NonnullOwnPtr<TestCertificateProvider>(adopt_own(*raw_provider)));

    chain.query(must_parse_url("https://example.com/"sv));
    chain.query(must_parse_url("https://example.com/"sv));

    EXPECT_EQ(raw_provider->call_count(), 1);
}

TEST_CASE(chain_different_origins_cached_separately)
{
    auto* raw_provider = new TestCertificateProvider("counting"sv,
        [](URL::URL const&) -> ClientCertificateResult {
            return { ClientCertificateResult::Outcome::Certificate, { "CERT"sv, "KEY"sv } };
        });

    ClientCertificateChain chain;
    chain.add_provider(NonnullOwnPtr<TestCertificateProvider>(adopt_own(*raw_provider)));

    chain.query(must_parse_url("https://a.com/"sv));
    chain.query(must_parse_url("https://b.com/"sv));

    EXPECT_EQ(raw_provider->call_count(), 2);
}

TEST_CASE(chain_different_ports_cached_separately)
{
    auto* raw_provider = new TestCertificateProvider("counting"sv,
        [](URL::URL const&) -> ClientCertificateResult {
            return { ClientCertificateResult::Outcome::Certificate, { "CERT"sv, "KEY"sv } };
        });

    ClientCertificateChain chain;
    chain.add_provider(NonnullOwnPtr<TestCertificateProvider>(adopt_own(*raw_provider)));

    chain.query(must_parse_url("https://example.com:443/"sv));
    chain.query(must_parse_url("https://example.com:8443/"sv));

    EXPECT_EQ(raw_provider->call_count(), 2);
}

TEST_CASE(chain_cache_invalidated_on_provider_add)
{
    auto* raw_provider = new TestCertificateProvider("counting"sv,
        [](URL::URL const&) -> ClientCertificateResult {
            return { ClientCertificateResult::Outcome::Certificate, { "CERT"sv, "KEY"sv } };
        });

    ClientCertificateChain chain;
    chain.add_provider(NonnullOwnPtr<TestCertificateProvider>(adopt_own(*raw_provider)));

    chain.query(must_parse_url("https://example.com/"sv));
    EXPECT_EQ(raw_provider->call_count(), 1);

    chain.add_provider(make_no_match_provider("new"sv));

    chain.query(must_parse_url("https://example.com/"sv));
    EXPECT_EQ(raw_provider->call_count(), 2);
}
