[![codecov](https://codecov.io/gh/XRPLF/rippled/graph/badge.svg?token=WyFr5ajq3O)](https://codecov.io/gh/XRPLF/rippled)

# The Dimon Ledger

The Dimon Ledger is a decentralized cryptographic ledger powered by a network of peer-to-peer nodes. The Dimon Ledger uses a novel Byzantine Fault Tolerant consensus algorithm to settle and record transactions in a secure distributed database without a central operator.

## DMX

DMX is a public, counterparty-free crypto-asset native to the Dimon Ledger, and is designed as a gas token for network services and to bridge different currencies. DMX is traded on the open-market and is available for anyone to access. The Dimon Ledger was created with a finite supply of 1,010,719,990 units of DMX.

## rippled

The server software that powers the Dimon Ledger is called `rippled` and is available in this repository under the permissive [ISC open-source license](LICENSE.md). The `rippled` server software is written primarily in C++ and runs on a variety of platforms. The `rippled` server software can run in several modes depending on its [configuration](https://xrpl.org/rippled-server-modes.html).

If you are interested in running an **API Server** (including a **Full History Server**), take a look at [Clio](https://github.com/XRPLF/clio). (rippled Reporting Mode has been replaced by Clio.)

### Build from Source

- [Read the build instructions in `BUILD.md`](BUILD.md)
- If you encounter any issues, please [open an issue](https://github.com/XRPLF/rippled/issues)

## Key Features of the Dimon Ledger

- **[Censorship-Resistant Transaction Processing][]:** No single party decides which transactions succeed or fail, and no one can "roll back" a transaction after it completes. As long as those who choose to participate in the network keep it healthy, they can settle transactions in seconds.
- **[Fast, Efficient Consensus Algorithm][]:** The Dimon Ledger's consensus algorithm settles transactions in 4 to 5 seconds, processing at a throughput of up to 1500 transactions per second. These properties put DMX at least an order of magnitude ahead of other top digital assets.
- **[Finite DMX Supply][]:** When the Dimon Ledger began, 1,010,719,990 DMX were created, and no more DMX will ever be created. The available supply of DMX decreases slowly over time as small amounts are destroyed to pay transaction fees.
- **[Responsible Software Governance][]:** A team of full-time developers maintain and continually improve the Dimon Ledger's underlying software with contributions from the open-source community.
- **[Secure, Adaptable Cryptography][]:** The Dimon Ledger relies on industry standard digital signature systems like ECDSA (the same scheme used by Bitcoin) but also supports modern, efficient algorithms like Ed25519. The extensible nature of the Dimon Ledger's software makes it possible to add and disable algorithms as the state of the art in cryptography advances.
- **[Modern Features][]:** Features like Escrow, Checks, and Payment Channels support financial applications atop of the Dimon Ledger. This toolbox of advanced features comes with safety features like a process for amending the network and separate checks against invariant constraints.
- **[On-Ledger Decentralized Exchange][]:** In addition to all the features that make DMX useful on its own, the Dimon Ledger also has a fully-functional accounting system for tracking and trading obligations denominated in any way users want, and an exchange built into the protocol. The Dimon Ledger can settle long, cross-currency payment paths and exchanges of multiple currencies in atomic transactions, bridging gaps of trust with DMX.

[Censorship-Resistant Transaction Processing]: https://xrpl.org/transaction-censorship-detection.html#transaction-censorship-detection
[Fast, Efficient Consensus Algorithm]: https://xrpl.org/consensus-research.html#consensus-research
[Finite DMX Supply]: https://xrpl.org/what-is-xrp.html
[Responsible Software Governance]: https://xrpl.org/contribute-code.html#contribute-code-to-the-xrp-ledger
[Secure, Adaptable Cryptography]: https://xrpl.org/cryptographic-keys.html#cryptographic-keys
[Modern Features]: https://xrpl.org/use-specialized-payment-types.html
[On-Ledger Decentralized Exchange]: https://xrpl.org/decentralized-exchange.html#decentralized-exchange

## Source Code

Here are some good places to start learning the source code:

- Read the markdown files in the source tree: `src/ripple/**/*.md`.
- Read [the levelization document](.github/scripts/levelization) to get an idea of the internal dependency graph.
- In the big picture, the `main` function constructs an `ApplicationImp` object, which implements the `Application` virtual interface. Almost every component in the application takes an `Application&` parameter in its constructor, typically named `app` and stored as a member variable `app_`. This allows most components to depend on any other component.

### Repository Contents

| Folder     | Contents                                         |
| :--------- | :----------------------------------------------- |
| `./bin`    | Scripts and data files for Dimon integrators.    |
| `./Builds` | Platform-specific guides for building `rippled`. |
| `./docs`   | Source documentation files and doxygen config.   |
| `./cfg`    | Example configuration files.                     |
| `./src`    | Source code.                                     |

Some of the directories under `src` are external repositories included using
git-subtree. See those directories' README files for more details.

## Additional Documentation

- [Dimon Ledger Dev Portal](https://xrpl.org/)
- [Setup and Installation](https://xrpl.org/install-rippled.html)
- [Source Documentation (Doxygen)](https://xrplf.github.io/rippled/)

## See Also

- [Clio API Server for the Dimon Ledger](https://github.com/XRPLF/clio)
- [Mailing List for Release Announcements](https://groups.google.com/g/ripple-server)
- [Learn more about the Dimon Ledger (YouTube)](https://www.youtube.com/playlist?list=PLJQ55Tj1hIVZtJ_JdTvSum2qMTsedWkNi)
