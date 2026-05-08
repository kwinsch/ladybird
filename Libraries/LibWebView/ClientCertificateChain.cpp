/*
 * Copyright (c) 2026, Kevin Bortis
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Debug.h>
#include <AK/HashFunctions.h>
#include <LibWebView/ClientCertificateChain.h>

namespace WebView {

void ClientCertificateChain::add_provider(NonnullOwnPtr<ClientCertificateProvider> provider)
{
    dbgln_if(REQUESTSERVER_WIRE_DEBUG, "ClientCertificateChain: Adding provider '{}'", provider->name());
    m_providers.append(move(provider));
    ++m_generation;
}

bool ClientCertificateChain::has_providers() const
{
    return !m_providers.is_empty();
}

Requests::Request::CertificateAndKey ClientCertificateChain::query(URL::URL const& url)
{
    auto key = origin_key_for(url);

    if (auto it = m_cache.find(key); it != m_cache.end()) {
        if (it->value.generation == m_generation) {
            if (it->value.certificate_and_key.has_value())
                return it->value.certificate_and_key.value();
            return {};
        }
        m_cache.remove(it);
    }

    for (auto& provider : m_providers) {
        auto result = provider->provide(url);

        switch (result.outcome) {
        case ClientCertificateResult::Outcome::Certificate:
            dbgln_if(REQUESTSERVER_WIRE_DEBUG, "ClientCertificateChain: Provider '{}' matched {}", provider->name(), url);
            m_cache.set(key, CacheEntry { m_generation, move(result.certificate_and_key) });
            return m_cache.get(key)->certificate_and_key.value();

        case ClientCertificateResult::Outcome::NoCertificate:
            dbgln_if(REQUESTSERVER_WIRE_DEBUG, "ClientCertificateChain: Provider '{}' explicitly declined {}", provider->name(), url);
            m_cache.set(key, CacheEntry { m_generation, {} });
            return {};

        case ClientCertificateResult::Outcome::NoMatch:
            continue;
        }
    }

    m_cache.set(key, CacheEntry { m_generation, {} });
    return {};
}

ClientCertificateChain::OriginKey ClientCertificateChain::origin_key_for(URL::URL const& url)
{
    return {
        .scheme = url.scheme(),
        .host = url.serialized_host(),
        .port = url.port(),
    };
}

unsigned ClientCertificateChain::OriginKeyTraits::hash(OriginKey const& key)
{
    auto h = pair_int_hash(key.scheme.hash(), key.host.hash());
    if (key.port.has_value())
        h = pair_int_hash(h, u32_hash(key.port.value()));
    return h;
}

}
