/*
 * Copyright (c) 2026, Kevin Bortis
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteString.h>
#include <AK/Vector.h>
#include <LibWebView/ClientCertificateProvider.h>

namespace WebView {

class CLIClientCertificateProvider final : public ClientCertificateProvider {
public:
    struct HostCertificate {
        ByteString host_pattern;
        ByteString certificate;
        ByteString key;
    };

    static ErrorOr<NonnullOwnPtr<CLIClientCertificateProvider>> create(Vector<HostCertificate>);

    virtual ClientCertificateResult provide(URL::URL const&) override;
    virtual StringView name() const override { return "CLI"sv; }

private:
    explicit CLIClientCertificateProvider(Vector<HostCertificate>);

    Vector<HostCertificate> m_host_certificates;
};

}
