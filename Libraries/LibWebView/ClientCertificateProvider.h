/*
 * Copyright (c) 2026, Kevin Bortis
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/StringView.h>
#include <LibRequests/Request.h>
#include <LibURL/URL.h>
#include <LibWebView/Export.h>

namespace WebView {

struct WEBVIEW_API ClientCertificateResult {
    enum class Outcome : u8 {
        NoMatch,       // Provider does not know this origin; try next provider.
        NoCertificate, // Provider explicitly has no certificate; stop chain, cache negative.
        Certificate,   // Provider has a certificate; stop chain, cache positive.
    };

    Outcome outcome { Outcome::NoMatch };
    Requests::Request::CertificateAndKey certificate_and_key {};
};

class WEBVIEW_API ClientCertificateProvider {
public:
    virtual ~ClientCertificateProvider() = default;
    virtual ClientCertificateResult provide(URL::URL const&) = 0;
    virtual StringView name() const = 0;
};

}
