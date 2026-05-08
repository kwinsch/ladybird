/*
 * Copyright (c) 2026, Kevin Bortis
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/String.h>
#include <AK/Vector.h>
#include <LibRequests/Request.h>
#include <LibURL/URL.h>
#include <LibWebView/ClientCertificateProvider.h>
#include <LibWebView/Export.h>

namespace WebView {

class WEBVIEW_API ClientCertificateChain {
public:
    ClientCertificateChain() = default;

    void add_provider(NonnullOwnPtr<ClientCertificateProvider>);
    bool has_providers() const;

    Requests::Request::CertificateAndKey query(URL::URL const&);

private:
    struct OriginKey {
        String scheme;
        String host;
        Optional<u16> port;

        bool operator==(OriginKey const&) const = default;
    };

    struct OriginKeyTraits : public DefaultTraits<OriginKey> {
        static unsigned hash(OriginKey const&);
    };

    struct CacheEntry {
        u64 generation { 0 };
        Optional<Requests::Request::CertificateAndKey> certificate_and_key;
    };

    static OriginKey origin_key_for(URL::URL const&);

    Vector<NonnullOwnPtr<ClientCertificateProvider>> m_providers;
    HashMap<OriginKey, CacheEntry, OriginKeyTraits> m_cache;
    u64 m_generation { 0 };
};

}
