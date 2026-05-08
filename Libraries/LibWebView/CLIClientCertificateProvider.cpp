/*
 * Copyright (c) 2026, Kevin Bortis
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWebView/CLIClientCertificateProvider.h>

namespace WebView {

ErrorOr<NonnullOwnPtr<CLIClientCertificateProvider>> CLIClientCertificateProvider::create(Vector<HostCertificate> host_certificates)
{
    VERIFY(!host_certificates.is_empty());
    return adopt_own(*new CLIClientCertificateProvider(move(host_certificates)));
}

CLIClientCertificateProvider::CLIClientCertificateProvider(Vector<HostCertificate> host_certificates)
    : m_host_certificates(move(host_certificates))
{
}

ClientCertificateResult CLIClientCertificateProvider::provide(URL::URL const& url)
{
    auto request_host = url.serialized_host();

    // Exact host match first.
    for (auto const& entry : m_host_certificates) {
        if (entry.host_pattern.view().equals_ignoring_ascii_case(request_host))
            return { ClientCertificateResult::Outcome::Certificate, { entry.certificate, entry.key } };
    }

    // Wildcard match only if no exact match found.
    for (auto const& entry : m_host_certificates) {
        if (entry.host_pattern == "*"sv)
            return { ClientCertificateResult::Outcome::Certificate, { entry.certificate, entry.key } };
    }

    return {};
}

}
