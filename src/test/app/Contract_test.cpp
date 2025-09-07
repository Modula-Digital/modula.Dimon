//------------------------------------------------------------------------------
/*
  This file is part of rippled: https://github.com/ripple/rippled
  Copyright (c) 2025 Ripple Labs Inc.

  Permission to use, copy, modify, and/or distribute this software for any
  purpose  with  or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
  MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <test/jtx.h>
#include <test/jtx/WSClient.h>

#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/jss.h>

namespace ripple {
namespace test {

class Contract_test : public beast::unit_test::suite
{
    struct TestLedgerData
    {
        int index;
        std::string txType;
        std::string result;
    };

    Json::Value
    getLastLedger(jtx::Env& env)
    {
        Json::Value params;
        params[jss::ledger_index] = env.closed()->seq();
        params[jss::transactions] = true;
        params[jss::expand] = true;
        return env.rpc("json", "ledger", to_string(params));
    }

    Json::Value
    getTxByIndex(Json::Value const& jrr, int const index)
    {
        for (auto const& txn : jrr[jss::result][jss::ledger][jss::transactions])
        {
            if (txn[jss::metaData][sfTransactionIndex.jsonName] == index)
                return txn;
        }
        return {};
    }

    void
    validateClosedLedger(
        jtx::Env& env,
        std::vector<TestLedgerData> const& ledgerResults)
    {
        auto const jrr = getLastLedger(env);
        auto const transactions =
            jrr[jss::result][jss::ledger][jss::transactions];
        BEAST_EXPECT(transactions.size() == ledgerResults.size());
        for (TestLedgerData const& ledgerResult : ledgerResults)
        {
            auto const txn = getTxByIndex(jrr, ledgerResult.index);
            BEAST_EXPECT(txn.isMember(jss::metaData));
            Json::Value const meta = txn[jss::metaData];
            BEAST_EXPECT(
                txn[sfTransactionType.jsonName] == ledgerResult.txType);
            BEAST_EXPECT(
                meta[sfTransactionResult.jsonName] == ledgerResult.result);
        }
    }

    static std::pair<uint256, std::shared_ptr<SLE const>>
    contractSourceKeyAndSle(ReadView const& view, uint256 const& contractHash)
    {
        auto const k = keylet::contractSource(contractHash);
        return {k.key, view.read(k)};
    }

    static std::pair<uint256, std::shared_ptr<SLE const>>
    contractKeyAndSle(
        ReadView const& view,
        uint256 const& contractHash,
        std::uint32_t const& seq)
    {
        auto const k = keylet::contract(contractHash, seq);
        return {k.key, view.read(k)};
    }

    Json::Value
    getContractCreateTx(Json::Value const& jrr)
    {
        for (auto const& txn : jrr[jss::result][jss::ledger][jss::transactions])
        {
            if (txn[jss::TransactionType] == jss::ContractCreate)
                return txn;
        }
        return {};
    }

    uint256
    getContractHash(Blob const& wasmBytes)
    {
        return ripple::sha512Half_s(
            ripple::Slice(wasmBytes.data(), wasmBytes.size()));
    }

    void
    validateFunctions(
        std::shared_ptr<SLE const> const& sle,
        Json::Value const& functions)
    {
        auto const stored = sle->getFieldArray(sfFunctions);
        BEAST_EXPECT(stored.size() == functions.size());
        for (std::size_t i = 0; i < stored.size(); ++i)
        {
            auto const sIPV = stored[i].getJson(JsonOptions::none);
            auto const& eIPV = functions[i]["Function"];

            // Compare function name.
            BEAST_EXPECT(sIPV.isMember("FunctionName"));
            BEAST_EXPECT(eIPV.isMember("FunctionName"));
            BEAST_EXPECT(
                sIPV["FunctionName"].asString() ==
                eIPV["FunctionName"].asString());

            // Compare parameters if present.
            if (eIPV.isMember("Parameters"))
            {
                BEAST_EXPECT(sIPV.isMember("Parameters"));
                BEAST_EXPECT(sIPV["Parameters"].isArray());
                BEAST_EXPECT(eIPV["Parameters"].isArray());
                BEAST_EXPECT(
                    sIPV["Parameters"].size() == eIPV["Parameters"].size());

                for (std::size_t j = 0; j < sIPV["Parameters"].size(); ++j)
                {
                    auto const& sParam = sIPV["Parameters"][j];
                    auto const& eParam = eIPV["Parameters"][j]["Parameter"];

                    // Compare ParameterFlag if present.
                    if (sParam.isMember("ParameterFlag"))
                    {
                        BEAST_EXPECT(eParam.isMember("ParameterFlag"));
                        BEAST_EXPECT(
                            sParam["ParameterFlag"].asUInt() ==
                            eParam["ParameterFlag"].asUInt());
                    }

                    // Compare ParameterName if present.
                    if (sParam.isMember("ParameterName"))
                    {
                        BEAST_EXPECT(eParam.isMember("ParameterName"));
                        BEAST_EXPECT(
                            sParam["ParameterName"].asString() ==
                            eParam["ParameterName"].asString());
                    }

                    // Compare ParameterType if present.
                    if (sParam.isMember("ParameterType"))
                    {
                        BEAST_EXPECT(eParam.isMember("ParameterType"));
                        BEAST_EXPECT(
                            sParam["ParameterType"]["type"].asString() ==
                            eParam["ParameterType"]["type"].asString());
                    }
                }
            }
        }
    }

    void
    validateInstanceParams(
        std::shared_ptr<SLE const> const& sle,
        Json::Value const& instanceParamValues)
    {
        // Convert stored SLE array to JSON and compare against expected JSON.
        auto const stored = sle->getFieldArray(sfInstanceParameterValues);
        BEAST_EXPECT(stored.size() == instanceParamValues.size());

        for (std::size_t i = 0; i < stored.size(); ++i)
        {
            // Convert the STObject entry to JSON for easy comparison.
            auto const sIPV = stored[i].getJson(JsonOptions::none);
            auto const& eIPV = instanceParamValues[i]["InstanceParameterValue"];

            // Compare flag if present.
            BEAST_EXPECT(sIPV.isMember("ParameterFlag"));
            BEAST_EXPECT(eIPV.isMember("ParameterFlag"));
            BEAST_EXPECT(
                sIPV["ParameterFlag"].asUInt() ==
                eIPV["ParameterFlag"].asUInt());

            // Compare ParameterValue contents (name/type/value) when present.
            BEAST_EXPECT(sIPV.isMember("ParameterValue"));
            BEAST_EXPECT(eIPV.isMember("ParameterValue"));
            auto const& sPV = sIPV["ParameterValue"];
            auto const& ePV = eIPV["ParameterValue"];

            if (ePV.isMember("name"))
                BEAST_EXPECT(
                    sPV.isMember("name") &&
                    sPV["name"].asString() == ePV["name"].asString());

            if (ePV.isMember("type"))
                BEAST_EXPECT(
                    sPV.isMember("type") &&
                    sPV["type"].asString() == ePV["type"].asString());

            if (ePV.isMember("value"))
            {
                // value can be number, string, or object; compare generically
                BEAST_EXPECT(sPV.isMember("value"));
                BEAST_EXPECT(sPV["value"] == ePV["value"]);
            }
        }
    }

    void
    validateInstanceParamValues(
        std::shared_ptr<SLE const> const& sle,
        Json::Value const& instanceParamValues)
    {
        // Convert stored SLE array to JSON and compare against expected JSON.
        auto const stored = sle->getFieldArray(sfInstanceParameterValues);
        BEAST_EXPECT(stored.size() == instanceParamValues.size());

        for (std::size_t i = 0; i < stored.size(); ++i)
        {
            // Convert the STObject entry to JSON for easy comparison.
            auto const sIPV = stored[i].getJson(JsonOptions::none);
            auto const& eIPV = instanceParamValues[i]["InstanceParameterValue"];

            // Compare flag if present.
            BEAST_EXPECT(sIPV.isMember("ParameterFlag"));
            BEAST_EXPECT(eIPV.isMember("ParameterFlag"));
            BEAST_EXPECT(
                sIPV["ParameterFlag"].asUInt() ==
                eIPV["ParameterFlag"].asUInt());

            // Compare ParameterValue contents (name/type/value) when present.
            BEAST_EXPECT(sIPV.isMember("ParameterValue"));
            BEAST_EXPECT(eIPV.isMember("ParameterValue"));
            auto const& sPV = sIPV["ParameterValue"];
            auto const& ePV = eIPV["ParameterValue"];

            if (ePV.isMember("type"))
                BEAST_EXPECT(
                    sPV.isMember("type") &&
                    sPV["type"].asString() == ePV["type"].asString());

            if (ePV.isMember("value"))
            {
                // value can be number, string, or object; compare generically
                BEAST_EXPECT(sPV.isMember("value"));
                BEAST_EXPECT(sPV["value"] == ePV["value"]);
            }
        }
    }

    void
    validateContract(
        jtx::Env& env,
        AccountID const& contractAccount,
        AccountID const& owner,
        std::uint32_t const& flags,
        std::uint32_t const& seq,
        uint256 const& contractHash,
        std::optional<Json::Value> const& instanceParamValues = std::nullopt,
        std::optional<std::string> const& uri = std::nullopt)
    {
        auto const [id, sle] =
            contractKeyAndSle(*env.current(), contractHash, seq);
        BEAST_EXPECT(sle);
        BEAST_EXPECT(sle->getAccountID(sfContractAccount) == contractAccount);
        BEAST_EXPECT(sle->getAccountID(sfOwner) == owner);
        BEAST_EXPECT(sle->getFieldU32(sfFlags) == flags);
        BEAST_EXPECT(sle->getFieldU32(sfSequence) == seq);
        BEAST_EXPECT(sle->getFieldH256(sfContractHash) == contractHash);
        if (instanceParamValues)
            validateInstanceParamValues(sle, *instanceParamValues);
        // if (uri)
        // {
        //     std::cout << "URI: " << *uri << std::endl;
        //     BEAST_EXPECT(sle->getFieldVL(sfURI) == strUnHex(*uri));
        // }
    }

    void
    validateContractSource(
        jtx::Env& env,
        Blob const& wasmBytes,
        uint256 const& contractHash,
        std::uint64_t const& referenceCount,
        Json::Value const& functions,
        std::optional<Json::Value> const& instanceParams = std::nullopt)
    {
        auto const [id, sle] =
            contractSourceKeyAndSle(*env.current(), contractHash);
        BEAST_EXPECT(sle);
        BEAST_EXPECT(sle->getFieldVL(sfContractCode) == wasmBytes);
        BEAST_EXPECT(sle->getFieldH256(sfContractHash) == contractHash);
        BEAST_EXPECT(sle->getFieldU64(sfReferenceCount) == referenceCount);
        validateFunctions(sle, functions);
    }

    template <typename... Args>
    std::tuple<jtx::Account, uint256, Json::Value>
    submitContract(jtx::Env& env, TER const& result, Args&&... args)
    {
        auto jt = env.jt(std::forward<Args>(args)...);
        env(jt, jtx::ter(result));
        env.close();

        // if (jt.jv.isMember(sfContractHash.jsonName))
        // {
        //     auto const accountID =
        //     parseBase58<AccountID>(jt.jv[sfContractAccount].asString());
        //     jtx::Account const contractAccount{
        //         "Contract pseudo-account",
        //         *accountID};
        //     return std::make_pair(contractAccount,
        //     uint256(jt.jv[sfContractHash]));
        // }

        auto const wasmBytes =
            strUnHex(jt.jv[sfContractCode.jsonName].asString());
        uint256 const contractHash = ripple::sha512Half_s(
            ripple::Slice(wasmBytes->data(), wasmBytes->size()));
        auto const [contractKey, sle] = contractKeyAndSle(
            *env.current(), contractHash, jt.jv[sfSequence.jsonName].asUInt());
        jtx::Account const contractAccount{
            "Contract pseudo-account", sle->getAccountID(sfContractAccount)};
        return std::make_tuple(contractAccount, contractHash, jt.jv);
    }

    std::string const BaseContractWasm =
        "0061736D01000000010E0260057F7F7F7F7F017F6000017F02120108686F73745F"
        "6C696205747261636500000302010105030100110619037F01418080C0000B7F00"
        "419E80C0000B7F0041A080C0000B072C04066D656D6F7279020004626173650001"
        "0A5F5F646174615F656E6403010B5F5F686561705F6261736503020A6C016A0101"
        "7F23808080800041206B2200248080808000200041186A410028009080C0800036"
        "0200200041106A410029008880C080003703002000410029008080C08000370308"
        "419480C08000410A200041086A411441011080808080001A200041206A24808080"
        "800041000B0B270100418080C0000B1EAE123A8556F3CF91154711376AFB0F894F"
        "832B3D20204163636F756E743A";

    std::string const Base2ContractWasm =
        "0061736D01000000010E0260057F7F7F7F7F017F6000017F02120108686F73745F6C69"
        "6205747261636500000302010105030100110619037F01418080C0000B7F0041A380C0"
        "000B7F0041B080C0000B072C04066D656D6F72790200046261736500010A5F5F646174"
        "615F656E6403010B5F5F686561705F6261736503020A1B011900418080C08000412341"
        "00410041001080808080001A41000B0B2C0100418080C0000B23242424242420535441"
        "5254494E47204241534520455845435554494F4E202424242424";

    void
    testCreateDoApply(FeatureBitset features)
    {
        testcase("create doApply");

        using namespace jtx;

        //----------------------------------------------------------------------
        // doApply.ContractCode.tesSUCCESS

        {
            test::jtx::Env env{*this, features};

            auto const alice = Account{"alice"};
            env.fund(XRP(10'000), alice);
            env.close();

            auto const seq = env.current()->seq();
            auto const [contractAccount, contractHash, jv] = submitContract(
                env, 
                tesSUCCESS, 
                contract::create(alice, BaseContractWasm),
                contract::add_instance_param(0, "uint8", "UINT8", 1),
                contract::add_function("base", {{0, "uint8", "UINT8"}}),
                token::uri("https://example.com/contract"),
                fee(XRP(200)));

            // validate contract
            validateContract(
                env,
                contractAccount.id(),
                alice.id(),
                0,
                seq,
                contractHash,
                jv[sfInstanceParameterValues],
                to_string(jv[sfURI]));

            // validate contract source
            // validateContractSource(
            //     env, *wasmBytes, contractHash, 1, jv[sfFunctions]);
        }

        //----------------------------------------------------------------------
        // doApply.ContractHash.tesSUCCESS

        {
            test::jtx::Env env{*this, features};

            auto const alice = Account{"alice"};
            env.fund(XRP(10'000), alice);
            env.close();

            // auto const wasmBytes = strUnHex(BaseContractWasm);
            // uint256 const contractHash = getContractHash(*wasmBytes);

            // Create Contract.
            {
                auto const seq = env.current()->seq();
                auto const [contractAccount, contractHash, jv] = submitContract(
                    env,
                    tesSUCCESS,
                    contract::create(alice, BaseContractWasm),
                    contract::add_instance_param(0, "uint8", "UINT8", 1),
                    contract::add_function("base", {{0, "uint8", "UINT8"}}),
                    fee(XRP(200)));

                // validate contract
                validateContract(
                    env,
                    contractAccount.id(),
                    alice.id(),
                    0,
                    seq,
                    contractHash,
                    jv[sfInstanceParameterValues]);

                // validate contract source
                // validateContractSource(
                //     env, *wasmBytes, contractHash, 1, jv[sfFunctions]);
            }

            // Install Contract.
            {
                auto const seq = env.current()->seq();

                auto const [contractAccount, contractHash, jv] = submitContract(
                    env,
                    tesSUCCESS,
                    contract::create(alice, BaseContractWasm),
                    contract::add_instance_param(0, "uint8", "UINT8", 1),
                    contract::add_function("base", {{0, "uint8", "UINT8"}}),
                    fee(XRP(200)));

                // validate contract
                validateContract(
                    env,
                    contractAccount.id(),
                    alice.id(),
                    0,
                    seq,
                    contractHash,
                    jv[sfInstanceParameterValues]);

                // validate contract source
                // validateContractSource(
                //     env, *wasmBytes, contractHash, 2, jv[sfFunctions]);
            }
        }
    }

    void
    testModifyDoApply(FeatureBitset features)
    {
        testcase("modify doApply");

        using namespace jtx;

        //----------------------------------------------------------------------
        // doApply.ContractCode.tesSUCCESS

        {
            test::jtx::Env env{*this, features};

            jtx::Account const alice = Account{"alice"};
            env.fund(XRP(10'000), alice);
            env.close();

            auto const [contractAccount, contractHash, jv] = submitContract(
                    env,
                    tesSUCCESS,
                    contract::create(alice, BaseContractWasm),
                    contract::add_instance_param(0, "uint8", "UINT8", 1),
                    contract::add_function("base", {{0, "uint8", "UINT8"}}),
                    fee(XRP(200)));

            env(contract::modify(alice, contractAccount, Base2ContractWasm),
                contract::add_instance_param(0, "uint16", "UINT16", 1),
                contract::add_function("base", {{0, "uint16", "UINT16"}}),
                fee(XRP(200)),
                ter(tesSUCCESS));
            env.close();

            // {
            //     Json::Value params;
            //     params[jss::ledger_index] = env.current()->seq() - 1;
            //     params[jss::transactions] = true;
            //     params[jss::expand] = true;
            //     auto const jrr = env.rpc("json", "ledger",
            //     to_string(params)); std::cout << jrr << std::endl;
            // }
        }

        //----------------------------------------------------------------------
        // doApply.ContractHash.tesSUCCESS

        {
            test::jtx::Env env{*this, features};

            auto const alice = Account{"alice"};
            env.fund(XRP(10'000), alice);
            env.close();

            auto const [contractAccount, contractHash, jv] = submitContract(
                    env,
                    tesSUCCESS,
                    contract::create(alice, BaseContractWasm),
                    contract::add_instance_param(0, "uint8", "UINT8", 1),
                    contract::add_function("base", {{0, "uint8", "UINT8"}}),
                    fee(XRP(200)));

            auto const seq = env.current()->seq();
            auto const [contractAccount2, contractHash2, jv2] = submitContract(
                    env,
                    tesSUCCESS,
                    contract::create(alice, Base2ContractWasm),
                    contract::add_instance_param(0, "uint8", "UINT8", 1),
                    contract::add_function("base", {{0, "uint8", "UINT8"}}),
                    fee(XRP(200)));

            // auto const wasmBytes = strUnHex(Base2ContractWasm);
            // uint256 const contractHash = ripple::sha512Half_s(
            //     ripple::Slice(wasmBytes->data(), wasmBytes->size()));
            auto const [contractId, contractSle] =
                contractKeyAndSle(*env.current(), contractHash, seq);
            auto const newContractHash =
                contractSle->getFieldH256(sfContractHash);

            env(contract::modify(alice, contractAccount, newContractHash),
                contract::add_instance_param(0, "uint16", "UINT16", 1),
                contract::add_function("base", {{0, "uint16", "UINT16"}}),
                fee(XRP(200)),
                ter(tesSUCCESS));
            env.close();

            {
                Json::Value params;
                params[jss::ledger_index] = env.current()->seq() - 1;
                params[jss::transactions] = true;
                params[jss::expand] = true;
                auto const jrr = env.rpc("json", "ledger", to_string(params));
                std::cout << jrr << std::endl;
            }
        }
    }

    void
    testDeleteDoApply(FeatureBitset features)
    {
        testcase("delete doApply");

        using namespace jtx;

        //----------------------------------------------------------------------
        {
            test::jtx::Env env{*this, features};

            jtx::Account const alice = Account{"alice"};
            env.fund(XRP(10'000), alice);
            env.close();

            auto const seq = env.current()->seq();
            auto const [contractAccount, contractHash, jv] = submitContract(
                env,
                tesSUCCESS,
                contract::create(alice, BaseContractWasm),
                contract::add_instance_param(0, "uint8", "UINT8", 1),
                contract::add_function("base", {{0, "uint8", "UINT8"}}),
                fee(XRP(200)));

            env(contract::del(alice, contractAccount), ter(tesSUCCESS));
            env.close();

            auto const wasmBytes = strUnHex(BaseContractWasm);
            auto const k = keylet::contract(contractHash, seq);
            BEAST_EXPECT(!env.le(k));

            {
                Json::Value params;
                params[jss::ledger_index] = env.current()->seq() - 1;
                params[jss::transactions] = true;
                params[jss::expand] = true;
                auto const jrr = env.rpc("json", "ledger", to_string(params));
                std::cout << jrr << std::endl;
            }
        }
    }

    std::string
    loadContractWasmStr(std::string const& contract_name = "")
    {
        std::string name =
            "/Users/darkmatter/projects/ledger-works/craft/projects/" +
            contract_name + "/target/wasm32-unknown-unknown/release/" +
            contract_name + ".wasm";
        if (!std::filesystem::exists(name))
        {
            std::cout << "File does not exist: " << name << "\n";
            return "";
        }

        std::ifstream file(name, std::ios::binary);

        if (!file)
        {
            std::cout << "Failed to open file: " << name << "\n";
            return "";
        }

        // Read the file into a vector
        std::vector<char> buffer(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        // Check if the buffer is empty
        if (buffer.empty())
        {
            std::cout << "File is empty or could not be read properly.\n";
            return "";
        }

        return strHex(buffer);
    }

    // void
    // testContractData(FeatureBitset features)
    // {
    //     testcase("contract data");

    //     using namespace jtx;

    //     test::jtx::Env env{*this, features};

    //     auto const alice = Account{"alice"};
    //     auto const bob = Account{"bob"};
    //     env.fund(XRP(10'000), alice, bob);
    //     env.close();

    //     // std::string contractWasmStr = loadContractWasmStr("contract_data");
    //     std::string contractWasmStr =
    //         "0061736D01000000013D0A60037F7F7E017F60057F7F7F7F7F017F60047F7F7F7F"
    //         "017F60037F7F7F017F60027F7F0060037F7F7F0060047F7F7F7F0060017F006000"
    //         "017F60000002610408686F73745F6C69620974726163655F6E756D000008686F73"
    //         "745F6C6962057472616365000108686F73745F6C6962116765745F636F6E747261"
    //         "63745F64617461000208686F73745F6C6962117365745F636F6E74726163745F64"
    //         "617461000203100F03040405060708040807090405060305030100110619037F01"
    //         "418080C0000B7F0041E982C0000B7F0041F082C0000B073705066D656D6F727902"
    //         "0006637265617465000A06757064617465000C0A5F5F646174615F656E6403010B"
    //         "5F5F686561705F6261736503020AFD310F2B01017F410021030240200220002D00"
    //         "C804470D0020004188046A200120021092808080004521030B20030B880101027F"
    //         "024020012D0080A401220241214F0D00200241D0046C2102200141B00B6A210102"
    //         "40034041002103024020020D000C020B200241B07B6A2102200141D0046A220141"
    //         "9480C080004105108480808000450D000B20012D00004103470D00200128020421"
    //         "02410121030B20002002360204200020033602000F0B2002412010868080800000"
    //         "0B0900108E80808000000B4101017F2380808080004190046B2203248080808000"
    //         "200341033A00082003200236020C200020014105200341086A1088808080002003"
    //         "4190046A2480808080000BC10101047F024020002D0080A401220441214F0D0020"
    //         "0041B00B6A21052000200441D0046C22066A4180106A2107024002400340200645"
    //         "0D01200641B07B6A2106200541D0046A220520012002108480808000450D000B41"
    //         "8804450D0120052003418804FC0A00000F0B20044120460D0002402002450D0020"
    //         "074188046A20012002FC0A00000B200720023A00C8040240418804450D00200720"
    //         "03418804FC0A00000B200020002D0080A40141016A3A0080A4010B0F0B20044120"
    //         "108680808000000B870201037F23808080800041D098016B220124808080800041"
    //         "002102024041C000450D002001419098016A410041C000FC0B000B024003402002"
    //         "41809401460D01200141086A20026A220341003A0000024041C704450D00200341"
    //         "016A2001418994016A41C704FC0A00000B200341C8046A41003A0000200241D004"
    //         "6A21020C000B0B0240418010450D0020004100418010FC0B000B200041013A0081"
    //         "A4012000410029008080C08000370082A4012000418AA4016A410029008880C080"
    //         "0037000020004192A4016A410028009080C08000360000024041809401450D0020"
    //         "004180106A200141086A41809401FC0A00000B200041003A0080A401200141D098"
    //         "016A2480808080000BA90201027F23808080800041B0A8016B2200248080808000"
    //         "200041106A108980808000200041106A419480C080004103108780808000200041"
    //         "106A419980C08000410C108780808000200041B9A4016A41002800B980C0800036"
    //         "0000200041B1A4016A41002900B180C08000370000200041002900A980C0800037"
    //         "00A9A4012000410A3A00A8A401200041106A419E80C08000410B200041A8A4016A"
    //         "108880808000200041086A200041106A108B808080000240024020002802084101"
    //         "71450D00200028020C21010C010B2000200041106A108580808000024020002802"
    //         "00410171450D0041BD80C0800041132000280204AD1080808080001A410021010C"
    //         "010B41D080C0800041194100410041001081808080001A417F21010B200041B0A8"
    //         "016A24808080800020010B8B1002107F017E23808080800041A0156B2202248080"
    //         "80800020012D0080A4012103200241D0006A4200370300200241C8006A42003703"
    //         "00200241C0006A420037030020024200370338410021040340024020032004470D"
    //         "0020034101200341014B1B210520014180106A2106410021074101210803400240"
    //         "0240024002400240024020082005460D00200841016A21092008411F4B210A2007"
    //         "210403402004417F460D06200A0D02200241386A20046A220B41016A220C2D0000"
    //         "220D41204F0D03200B2D0000210E200241306A2006200D41D0046C6A220F418804"
    //         "6A200F2D00C804109080808000200E41204F0D042002280234210F200228023021"
    //         "10200241286A2006200E41D0046C6A22114188046A20112D00C804109080808000"
    //         "20102002280228200F200228022C2211200F2011491B1092808080002210200F20"
    //         "116B20101B417F4A0D06200B200D3A0000200C200E3A00002004417F6A21040C00"
    //         "0B0B4100210F0240418010450D00200241D8006A4100418010FC0B000B41002111"
    //         "034002400240024002400240024002400240024002400240024002400240024002"
    //         "40024002400240024002400240024002402003200F460D00200F4120460D012002"
    //         "41386A200F6A2D0000220441204F0D162006200441D0046C6A220E2D00C8042104"
    //         "024041C000450D00200241D8106A410041C000FC0B000B200441C1004F0D142002"
    //         "41206A200E4188046A20041090808080002002280224220D2004470D1502402004"
    //         "450D00200241D8106A20022802202004FC0A00000B200E41016A210D200E2D0000"
    //         "210B0240418704450D0020024198116A200D418704FC0A00000B200241186A2002"
    //         "41D8106A200410908080800020022802182110200241106A200241D8006A201120"
    //         "0228021C22041091808080002002280214210E20022802104101710D1002402004"
    //         "450D00200241D8006A20116A200E6A20102004FC0A00000B200420116A200E6A21"
    //         "04417D210E200441FF0F4B0D10200241D8006A20046A41003A0000200441FF0F46"
    //         "0D10200441016A21114171210E200B0E0E1002030405060708090A0B0C0D0E100B"
    //         "0240201141C101490D00201141C1E100490D114174210E201141D989384F0D1020"
    //         "01201141BF9E7F6A22043A0002200120044108763A00012001200441107641716A"
    //         "3A00004103210F0C120B200120113A00004101210F0C110B4120108D8080800000"
    //         "0B200241D8006A20116A220E41103A0000200E20022D0098113A00014102210E0C"
    //         "0C0B200241D8006A20116A220E41013A0000200E20022F009911220D410874200D"
    //         "410876723B00014103210E0C0B0B200241D8006A20116A220D41023A0000200D20"
    //         "0228009B11220E411874200E4180FE037141087472200E4108764180FE0371200E"
    //         "41187672723600014105210E0C0A0B200241D8006A20116A220E41033A0000200E"
    //         "200229009F11221242388620124280FE0383422886842012428080FC0783421886"
    //         "201242808080F80F834208868484201242088842808080F80F8320124218884280"
    //         "80FC07838420124228884280FE038320124238888484843700014109210E0C090B"
    //         "200241D8006A20116A220E41043A0000200E200D290000370001200E41096A200D"
    //         "41086A2900003700004111210E0C080B200241D8006A20116A220E41113A000020"
    //         "0E200D290000370001200E41096A200D41086A290000370000200E41116A200D41"
    //         "106A2800003600004115210E0C070B200241D8006A20116A220E41053A0000200E"
    //         "200D290000370001200E41096A200D41086A290000370000200E41116A200D4110"
    //         "6A290000370000200E41196A200D41186A2900003700004121210E0C060B200241"
    //         "D8006A20116A220E41063A0000200E2002290398113700014109210E0C050B2002"
    //         "41D8006A20116A220B41073A0000200241086A200241D8006A200441026A20022F"
    //         "009915220D109180808000200228020C210E20022802084101710D05200E41016A"
    //         "210E0240200D450D00200B200E6A20024198116A200DFC0A00000B200E200D6A21"
    //         "0E0C040B200241D8006A20116A220E4188283B0000200E200D290000370002200E"
    //         "410A6A200D41086A290000370000200E41126A200D41106A280000360000411621"
    //         "0E0C030B200241D8006A20116A220E411A3A0000200E200D290000370001200E41"
    //         "096A200D41086A290000370000200E41116A200D41106A2800003600004115210E"
    //         "0C020B200241D8006A20116A220D410E3A0000024020022F009915220E450D0020"
    //         "0D41016A20024198116A200EFC0A00000B200E41016A210E0C010B200241D8006A"
    //         "20116A220D410F3A0000024020022F009915220E450D00200D41016A2002419811"
    //         "6A200EFC0A00000B200E41016A210E0B2002200241D8006A2004200E1091808080"
    //         "002002280200410171450D072002280204210E0B410121040C020B2001201141BF"
    //         "7E6A22043A00012001200441087641416A3A00004102210F0B02402011450D0020"
    //         "01200F6A200241D8006A2011FC0A00000B410121044171210E20012D0081A40141"
    //         "01470D00200F20116A22044181104F0D0920014182A4016A411420012004108380"
    //         "8080001A410021040B2000200E36020420002004360200200241A0156A24808080"
    //         "80000F0B200441C000108680808000000B2004200D108F80808000000B2004108D"
    //         "80808000000B200F41016A210F200E20116A21110C000B0B2008108D8080800000"
    //         "0B200D108D80808000000B200E108D80808000000B200441801010868080800000"
    //         "0B200741016A2107200921080C000B0B024020044120460D00200241386A20046A"
    //         "20043A0000200441016A21040C010B0B4120108D80808000000B961702127F017E"
    //         "23808080800041B0B4016B2200248080808000200041106A108980808000024002"
    //         "4020002D0091A4014101470D0020004192A4016A4114200041106A418010108280"
    //         "80800022014100480D01200041003A0090A40102400240024020002D0010220241"
    //         "C101490D0002400240200241F101490D00200241FF01470D010C040B20002D0011"
    //         "2002413F6A41FF01714108747241C1016A2102410221030C020B20002D00114108"
    //         "742002410F6A41FF01714110747220002D00127241C1E1006A2102410321030C01"
    //         "0B410121030B200320026A220420014B21024175210120020D02200041106A4180"
    //         "106A2105200041B0A8016A41146A2106200041B0AC016A41146A2107200041B0B0"
    //         "016A41146A21084100210902400340024002400240200320044F0D00200341FF0F"
    //         "4B0D04200041106A20036A22022D0000220A41C101490D010240200A41F101490D"
    //         "00200A41FF01460D06200341FD0F4B0D0520022D0001410874200A410F6A41FF01"
    //         "714110747220022D00027241C1E1006A210A410321020C030B200341FF0F460D04"
    //         "20022D0001200A413F6A41FF01714108747241C1016A210A410221020C020B4175"
    //         "210120032004470D06200041106A419480C080004104108780808000200041086A"
    //         "200041106A108B8080800002402000280208410171450D00200028020C21010C07"
    //         "0B2000200041106A10858080800002402000280200410171450D0041BD80C08000"
    //         "41132000280204AD1080808080001A410021010C070B41D080C080004119410041"
    //         "0041001081808080001A417F21010C060B410121020B200220036A2203200A6A22"
    //         "0B4180104B0D01200041106A20036A21020240200A450D004100200A41796A2203"
    //         "2003200A4B1B210C200241036A417C7120026B210D410021030340024002400240"
    //         "0240200220036A2D0000220EC0220F4100480D00200D20036B4103710D01200320"
    //         "0C4F0D020340200220036A220141046A280200200128020072418081828478710D"
    //         "03200341086A2203200C490D000C030B0B41752101024002400240024002400240"
    //         "024002400240200E41E980C080006A2D0000417E6A0E03000201120B200341016A"
    //         "2203200A4F0D11200220036A2C000041BF7F4C0D070C110B200341016A2210200A"
    //         "4F0D10200220106A2C00002110200E41907E6A0E050201010103010B200341016A"
    //         "2210200A4F0D0F200220106A2C00002110024002400240200E41E001460D00200E"
    //         "41ED01460D01200F411F6A41FF0171410C490D02200F417E71416E470D12201041"
    //         "40480D070C120B201041607141A07F460D060C110B2010419F7F4A0D100C050B20"
    //         "104140480D040C0F0B200F410F6A41FF017141024B0D0E20104140480D020C0E0B"
    //         "201041F0006A41FF01714130490D010C0D0B2010418F7F4A0D0C0B200341026A22"
    //         "0E200A4F0D0B2002200E6A2C000041BF7F4A0D0B200341036A2203200A4F0D0B20"
    //         "0220036A2C000041BF7F4A0D0B0C010B200341026A2203200A4F0D0A200220036A"
    //         "2C000041BF7F4A0D0A0B200341016A21030C020B200341016A21030C010B200320"
    //         "0A4F0D000340200220036A2C00004100480D01200A200341016A2203470D000C03"
    //         "0B0B2003200A490D000B0B200B418010460D0102400240200041106A200B6A2203"
    //         "2D0000220C41C101490D000240200C41F101490D00200C41FF01460D05200B41FD"
    //         "0F4B0D0420032D0001410874200C410F6A41FF01714110747220032D00027241C1"
    //         "E1006A210C410321030C020B200B41FF0F460D0320032D0001200C413F6A41FF01"
    //         "714108747241C1016A210C410221030C010B410121030B417321012003200B6A22"
    //         "0E200C6A22034180104B0D04200C450D03200C417F6A210C200E41016A210B416F"
    //         "210102400240024002400240024002400240024002400240024002400240024002"
    //         "40024002400240200041106A200E6A220F2D0000417F6A0E1A050607080A0B0001"
    //         "17171717170203040917171717171717170E170B41732101200E41FE0F4B0D1620"
    //         "0041106A200B6A220D2D0000220F41C101490D0B0240200F41F101490D00200F41"
    //         "FF01460D15200E41FC0F4B0D17200D2D0001410874200F410F6A41FF0171411074"
    //         "72200D2D00027241C1E1006A210F4103210E0C0D0B200B41FF0F460D16200D2D00"
    //         "01200F413F6A41FF01714108747241C1016A210F4102210E0C0C0B200C4115470D"
    //         "15200041106A200B6A2D00004114470D152000200F290002370398A8012000200F"
    //         "41096A29000037009FA801200F2800122111200F2D0011210E410A210B0C0D0B20"
    //         "0C4180044D0D0D0C0F0B200C4180044B0D0E0240418004450D00200041B0B0016A"
    //         "4100418004FC0B000B0240200C450D00200041B0B0016A200041106A200B6A200C"
    //         "FC0A00000B2000200041B0B0016A41076A29000037009FA801200020002900B0B0"
    //         "01370398A80120002D00BFB001210E20002800C0B0012111024041EC03450D0020"
    //         "0041A8A4016A200841EC03FC0A00000B410D210B200C210F0C0D0B200C4101470D"
    //         "122000200041106A200B6A2D00003A0098A8014101210B0C0A0B200C4102470D11"
    //         "2000200041106A200B6A2F000022014108742001410876723B0099A8014102210B"
    //         "0C090B200C4104470D102000200041106A200B6A280000220141187420014180FE"
    //         "03714108747220014108764180FE03712001411876727236009BA8014103210B0C"
    //         "080B200C4108470D0F2000200041106A200B6A290000221242388620124280FE03"
    //         "83422886842012428080FC0783421886201242808080F80F834208868484201242"
    //         "088842808080F80F832012421888428080FC07838420124228884280FE03832012"
    //         "42388884848437009FA8014104210B0C070B200C4110470D0E2000200041106A20"
    //         "0B6A2201290000370398A8012000200141076A29000037009FA80120012D000F21"
    //         "0E4105210B0C060B200C4114470D0D2000200041106A200B6A2201290000370398"
    //         "A8012000200141076A29000037009FA8012001280010211120012D000F210E4106"
    //         "210B0C050B200C4120470D0C200041A8A4016A41086A200041106A200B6A220141"
    //         "1C6A28000036020020002001290000370398A801200020012900143703A8A40141"
    //         "07210B2000200141076A29000037009FA8012001280010211120012D000F210E0C"
    //         "040B200C4108470D0B2000200041106A200B6A290000370398A8014108210B0C03"
    //         "0B4101210E0B41752101200F4180044B0D09200E200F6A200C4B0D090240418004"
    //         "450D00200041B0A8016A4100418004FC0B000B0240200F450D00200041B0A8016A"
    //         "200D200E6A200FFC0A00000B2000200041B0A8016A41076A29000037009FA80120"
    //         "0020002900B0A801370398A80120002D00BFA801210E20002800C0A80121110240"
    //         "41EC03450D00200041A8A4016A200641EC03FC0A00000B4109210B0C030B200C41"
    //         "14470D082000200041106A200B6A2201290000370398A8012000200141076A2900"
    //         "0037009FA8012001280010211120012D000F210E410B210B0B0C010B0240418004"
    //         "450D00200041B0AC016A4100418004FC0B000B0240200C450D00200041B0AC016A"
    //         "200041106A200B6A200CFC0A00000B2000200041B0AC016A41076A29000037009F"
    //         "A801200020002900B0AC01370398A80120002D00BFAC01210E20002800C0AC0121"
    //         "11024041EC03450D00200041A8A4016A200741EC03FC0A00000B410C210B200C21"
    //         "0F0B200041ACA8016A41026A220C20002D009AA8013A0000200020002F0198A801"
    //         "3B01ACA8010240200941FF01712201411F4D0D00417821010C060B200A41C1004F"
    //         "0D00200028009BA801210D200029009FA80121122005200141D0046C6A21010240"
    //         "200A450D0020014188046A2002200AFC0A00000B2001200B3A00002001200A3A00"
    //         "C804200120113600112001200E3A0010200120123703082001200D360204200120"
    //         "002F01ACA8013B0001200141036A200C2D00003A0000024041ED03450D00200141"
    //         "156A200041A8A4016A41ED03FC0A00000B2001200F3B018204200020002D0090A4"
    //         "0141016A22093A0090A4010C010B0B417421010C030B417321010C020B41752101"
    //         "0C010B417121010B200041B0B4016A24808080800020010B0900108E8080800000"
    //         "0B0300000B0900108E80808000000B27000240200241C100490D00200241C00010"
    //         "8680808000000B20002002360204200020013602000BD10101027F41012104417D"
    //         "21050240200241FF0F4B0D000240200341C101490D000240200341C1E100490D00"
    //         "0240200341D98938490D00417421050C030B200120026A2201200341BF9E7F6A22"
    //         "0341107641716A3A0000200241FD0F4B0D022001200341087420034180FE037141"
    //         "0876723B000141002104410321050C020B200120026A2201200341BF7E6A220341"
    //         "087641416A3A0000200241FF0F460D01200120033A000141002104410221050C01"
    //         "0B200120026A20033A000041002104410121050B20002005360204200020043602"
    //         "000B4A01037F4100210302402002450D000240034020002D0000220420012D0000"
    //         "2205470D01200041016A2100200141016A21012002417F6A2202450D020C000B0B"
    //         "200420056B21030B20030B0BF3020100418080C0000BE902AE123A8556F3CF9115"
    //         "4711376AFB0F894F832B3D636F756E74746F74616C64657374696E6174696F6E05"
    //         "96915CFDEEE3A695B3EFD6BDA9AC788A368B7B52656164206261636B20636F756E"
    //         "743A207B7D4661696C656420746F2072656164206261636B20636F756E74010101"
    //         "010101010101010101010101010101010101010101010101010101010101010101"
    //         "010101010101010101010101010101010101010101010101010101010101010101"
    //         "010101010101010101010101010101010101010101010101010101010101010101"
    //         "010101010101010101010101010101010101010101010101010100000000000000"
    //         "000000000000000000000000000000000000000000000000000000000000000000"
    //         "000000000000000000000000000000000000000000000000000002020202020202"
    //         "020202020202020202020202020202020202020202020203030303030303030303"
    //         "03030303030304040404040000000000000000000000";

    //     env(contract::create(alice, contractWasmStr),
    //         contract::add_instance_param(
    //             tfSendAmount, "value", "AMOUNT", XRP(2000)),
    //         contract::add_function("create", {}),
    //         contract::add_function("update", {}),
    //         fee(XRP(200)),
    //         ter(tesSUCCESS));
    //     env.close();

    //     {
    //         Json::Value params;
    //         params[jss::ledger_index] = env.current()->seq() - 1;
    //         params[jss::transactions] = true;
    //         params[jss::expand] = true;
    //         auto const jrr = env.rpc("json", "ledger", to_string(params));
    //         std::cout << jrr << std::endl;
    //     }

    //     auto const contractAccount = getContractOwner(env);
    //     env(contract::call(alice, contractAccount, "create"),
    //         escrow::comp_allowance(1'000'000),
    //         ter(tesSUCCESS));
    //     env.close();

    //     {
    //         Json::Value params;
    //         params[jss::ledger_index] = env.current()->seq() - 1;
    //         params[jss::transactions] = true;
    //         params[jss::expand] = true;
    //         auto const jrr = env.rpc("json", "ledger", to_string(params));
    //         std::cout << jrr << std::endl;
    //     }

    //     {
    //         // Get contract info
    //         Json::Value params;
    //         params[jss::contract_account] = contractAccount;
    //         params[jss::account] = alice.human();
    //         auto const jrr =
    //             env.rpc("json", "contract_info", to_string(params));
    //         std::cout << jrr << std::endl;
    //     }

    //     env(contract::call(alice, contractAccount, "update"),
    //         escrow::comp_allowance(1000000),
    //         ter(tesSUCCESS));
    //     env.close();

    //     {
    //         Json::Value params;
    //         params[jss::ledger_index] = env.current()->seq() - 1;
    //         params[jss::transactions] = true;
    //         params[jss::expand] = true;
    //         auto const jrr = env.rpc("json", "ledger", to_string(params));
    //         std::cout << jrr << std::endl;
    //     }

    //     {
    //         // Get contract info
    //         Json::Value params;
    //         params[jss::contract_account] = contractAccount;
    //         params[jss::account] = alice.human();
    //         auto const jrr =
    //             env.rpc("json", "contract_info", to_string(params));
    //         std::cout << jrr << std::endl;
    //     }
    // }

    // void
    // testContractDataV2(FeatureBitset features)
    // {
    //     testcase("contract data v2");

    //     using namespace jtx;

    //     test::jtx::Env env{*this, features};

    //     auto const alice = Account{"alice"};
    //     auto const bob = Account{"bob"};
    //     env.fund(XRP(10'000), alice, bob);
    //     env.close();

    //     // std::string contractWasmStr =
    //     // loadContractWasmStr("contract_data_v2");
    //     std::string contractWasmStr =
    //         "0061736D0100000001360760067F7F7F7F7F7F017F60087F7F7F7F7F7F7F7F017F"
    //         "60037F7F7E017F60057F7F7F7F7F017F60027F7F0060037F7F7F006000017F02CD"
    //         "010608686F73745F6C69621A6765745F636F6E74726163745F646174615F66726F"
    //         "6D5F6B6579000008686F73745F6C69621A7365745F636F6E74726163745F646174"
    //         "615F66726F6D5F6B6579000008686F73745F6C6962217365745F6E65737465645F"
    //         "636F6E74726163745F646174615F66726F6D5F6B6579000108686F73745F6C6962"
    //         "0974726163655F6E756D000208686F73745F6C6962216765745F6E65737465645F"
    //         "636F6E74726163745F646174615F66726F6D5F6B6579000108686F73745F6C6962"
    //         "05747261636500030305040405060605030100110619037F01418080C0000B7F00"
    //         "41AC81C0000B7F0041B081C0000B073705066D656D6F7279020006637265617465"
    //         "00080675706461746500090A5F5F646174615F656E6403010B5F5F686561705F62"
    //         "61736503020ABB0604860101037F23808080800041106B22022480808080004100"
    //         "21032002410036020C024020014114419480C0800041052002410C6A4104108080"
    //         "8080004104470D00200228020C220341187420034180FE03714108747220034108"
    //         "764180FE0371200341187672722104410121030B20002004360204200020033602"
    //         "00200241106A2480808080000B4401017F23808080800041106B22032480808080"
    //         "00200320023A000F2003410236000B20004114200141052003410B6A4105108180"
    //         "8080001A200341106A2480808080000BB20301027F23808080800041C0006B2200"
    //         "248080808000200041206A410028009080C08000360200200041186A4100290088"
    //         "80C080003703002000410029008080C08000370310200041106A419480C0800041"
    //         "03108780808000200041106A419980C08000410C10878080800020004190183B00"
    //         "2A200041106A4114419E80C08000410341A180C0800041062000412A6A41021082"
    //         "808080001A200041346A41002900BA80C080003700002000413C6A41002800C280"
    //         "C0800036000020004188283B002A200041002900B280C0800037002C200041106A"
    //         "411441A780C08000410B2000412A6A41161081808080001A200041086A20004110"
    //         "6A1086808080000240024002402000280208410171450D0041C680C08000411320"
    //         "0028020CAD1083808080001A200041003A002A0240200041106A4114419E80C080"
    //         "00410341A180C0800041062000412A6A41011084808080004101470D0041D980C0"
    //         "8000411A200031002A1083808080001A410021010C030B41F380C0800041204100"
    //         "410041001085808080001A0C010B419381C0800041194100410041001085808080"
    //         "001A0B417F21010B200041C0006A24808080800020010BB70101027F2380808080"
    //         "0041206B220024808080800041002101200041186A410028009080C08000360200"
    //         "200041106A410029008880C080003703002000410029008080C080003703082000"
    //         "41086A419480C0800041041087808080002000200041086A108680808000024002"
    //         "402000280200410171450D0041C680C0800041132000280204AD1083808080001A"
    //         "0C010B419381C0800041194100410041001085808080001A417F21010B20004120"
    //         "6A24808080800020010B0BB6010100418080C0000BAC01AE123A8556F3CF911547"
    //         "11376AFB0F894F832B3D636F756E74746F74616C6B65797375626B657964657374"
    //         "696E6174696F6E0596915CFDEEE3A695B3EFD6BDA9AC788A368B7B526561642062"
    //         "61636B20636F756E743A207B7D52656164206261636B206E65737465642076616C"
    //         "75653A207B7D4661696C656420746F2072656164206261636B206E657374656420"
    //         "76616C75654661696C656420746F2072656164206261636B20636F756E74";

    //     env(contract::create(alice, contractWasmStr),
    //         contract::add_instance_param(
    //             tfSendAmount, "value", "AMOUNT", XRP(2000)),
    //         contract::add_function("create", {}),
    //         contract::add_function("update", {}),
    //         fee(XRP(200)),
    //         ter(tesSUCCESS));
    //     env.close();

    //     {
    //         Json::Value params;
    //         params[jss::ledger_index] = env.current()->seq() - 1;
    //         params[jss::transactions] = true;
    //         params[jss::expand] = true;
    //         auto const jrr = env.rpc("json", "ledger", to_string(params));
    //         std::cout << jrr << std::endl;
    //     }

    //     auto const contractAccount = getContractOwner(env);
    //     env(contract::call(alice, contractAccount, "create"),
    //         escrow::comp_allowance(1'000'000),
    //         ter(tesSUCCESS));
    //     env.close();

    //     {
    //         Json::Value params;
    //         params[jss::ledger_index] = env.current()->seq() - 1;
    //         params[jss::transactions] = true;
    //         params[jss::expand] = true;
    //         auto const jrr = env.rpc("json", "ledger", to_string(params));
    //         std::cout << jrr << std::endl;
    //     }

    //     {
    //         // Get contract info
    //         Json::Value params;
    //         params[jss::contract_account] = contractAccount;
    //         params[jss::account] = alice.human();
    //         auto const jrr =
    //             env.rpc("json", "contract_info", to_string(params));
    //         std::cout << jrr << std::endl;
    //     }

    //     env(contract::call(alice, contractAccount, "update"),
    //         escrow::comp_allowance(1000000),
    //         ter(tesSUCCESS));
    //     env.close();

    //     {
    //         Json::Value params;
    //         params[jss::ledger_index] = env.current()->seq() - 1;
    //         params[jss::transactions] = true;
    //         params[jss::expand] = true;
    //         auto const jrr = env.rpc("json", "ledger", to_string(params));
    //         std::cout << jrr << std::endl;
    //     }

    //     {
    //         // Get contract info
    //         Json::Value params;
    //         params[jss::contract_account] = contractAccount;
    //         params[jss::account] = alice.human();
    //         auto const jrr =
    //             env.rpc("json", "contract_info", to_string(params));
    //         std::cout << jrr << std::endl;
    //     }
    // }

    // void
    // testContractDataAdvanced(FeatureBitset features)
    // {
    //     testcase("contract data advanced");

    //     using namespace jtx;

    //     test::jtx::Env env{*this, features};

    //     auto const alice = Account{"alice"};
    //     auto const bob = Account{"bob"};
    //     env.fund(XRP(10'000), alice, bob);
    //     env.close();

    //     std::string contractWasmStr =
    //         loadContractWasmStr("contract_data_advanced");

    //     env(contract::create(alice, contractWasmStr),
    //         contract::add_instance_param(
    //             tfSendAmount, "value", "AMOUNT", XRP(2000)),
    //         contract::add_function(
    //             "test",
    //             {
    //                 {0, "account", "ACCOUNT"},
    //                 {0, "uint32", "UINT32"},
    //             }),
    //         fee(XRP(200)),
    //         ter(tesSUCCESS));
    //     env.close();

    //     {
    //         Json::Value params;
    //         params[jss::ledger_index] = env.current()->seq() - 1;
    //         params[jss::transactions] = true;
    //         params[jss::expand] = true;
    //         auto const jrr = env.rpc("json", "ledger", to_string(params));
    //         std::cout << jrr << std::endl;
    //     }

    //     auto const contractAccount = getContractOwner(env);
    //     env(contract::call(alice, contractAccount, "test"),
    //         contract::add_param(0, "account", "ACCOUNT", alice.human()),
    //         contract::add_param(0, "uint32", "UINT32", 5),
    //         escrow::comp_allowance(1'000'000),
    //         ter(tesSUCCESS));
    //     env.close();

    //     {
    //         Json::Value params;
    //         params[jss::ledger_index] = env.current()->seq() - 1;
    //         params[jss::transactions] = true;
    //         params[jss::expand] = true;
    //         auto const jrr = env.rpc("json", "ledger", to_string(params));
    //         std::cout << jrr << std::endl;
    //     }

    //     {
    //         // Get contract info
    //         Json::Value params;
    //         params[jss::contract_account] = contractAccount;
    //         params[jss::account] = alice.human();
    //         auto const jrr =
    //             env.rpc("json", "contract_info", to_string(params));
    //         std::cout << jrr << std::endl;
    //     }
    // }

    // void
    // testParameters(FeatureBitset features)
    // {
    //     testcase("parameters");

    //     using namespace jtx;

    //     // Env env{*this, envconfig(), features, nullptr,
    //     //     beast::severities::kTrace
    //     // };
    //     Env env{*this, features};

    //     auto const alice = Account{"alice"};
    //     auto const bob = Account{"bob"};
    //     auto const gw = Account{"gw"};
    //     auto const USD = gw["USD"];
    //     env.fund(XRP(10'000), alice, bob);
    //     env.close();

    //     std::string contractWasmStr = loadContractWasmStr("parameters");
    //     // std::string contractWasmStr =
    //     //     "0061736D0100000001300760047F7F7F7F017F60037F7F7E017F60057F7F7F7F7F"
    //     //     "017F60077F7F7F7F7F7F7F017F6000017F60027F7F0060000002750508686F7374"
    //     //     "5F6C69620F6F74786E5F63616C6C5F706172616D000008686F73745F6C69620974"
    //     //     "726163655F6E756D000108686F73745F6C6962057472616365000208686F73745F"
    //     //     "6C69621274726163655F6F70617175655F666C6F6174000008686F73745F6C6962"
    //     //     "09666C6F61745F616464000303040304050605030100110619037F01418080C000"
    //     //     "0B7F0041DB84C0000B7F0041E084C0000B072C04066D656D6F727902000463616C"
    //     //     "6C00050A5F5F646174615F656E6403010B5F5F686561705F6261736503020AD911"
    //     //     "03C81103037F017E017F23808080800041B0026B2200248080808000200041003A"
    //     //     "0001418080C08000411041004110200041016A4101108080808000AC1081808080"
    //     //     "001A419080C08000410C20003100011081808080001A419C80C08000410A200041"
    //     //     "016A410141011082808080001A200041003B010241A680C0800041114101410120"
    //     //     "0041026A4102108080808000AC1081808080001A41B780C08000410D2000330102"
    //     //     "1081808080001A41C480C08000410B200041026A410241011082808080001A2000"
    //     //     "410036020441CF80C08000411141024102200041046A4104108080808000AC1081"
    //     //     "808080001A41E080C08000410D20003502041081808080001A41ED80C08000410B"
    //     //     "200041046A410441011082808080001A2000420037030841F880C0800041114103"
    //     //     "4103200041086A4108108080808000AC1081808080001A418981C08000410D2000"
    //     //     "2903081081808080001A419681C08000410B200041086A41084101108280808000"
    //     //     "1A200042003703182000420037031041A181C08000411241044104200041106A41"
    //     //     "10108080808000AC1081808080001A41B381C08000410E20002903101081808080"
    //     //     "001A41C181C08000410C200041106A411041011082808080001A200041206A4110"
    //     //     "6A4100360200200042003703282000420037032041CD81C0800041124105411120"
    //     //     "0041206A4114108080808000AC1081808080001A41DF81C08000410E2000290320"
    //     //     "1081808080001A41ED81C08000410C200041206A411441011082808080001A2000"
    //     //     "41C0006A41106A4200370300200042003703482000420037034041F981C0800041"
    //     //     "1241064115200041C0006A4118108080808000AC1081808080001A418B82C08000"
    //     //     "410E20002903401081808080001A419982C08000410C200041C0006A4118410110"
    //     //     "82808080001A200041E0006A41186A4200370300200041E0006A41106A42003703"
    //     //     "00200041E0006A41086A42003703002000420037036041A582C080004112410741"
    //     //     "05200041E0006A4120108080808000AC1081808080001A41B782C08000410C2000"
    //     //     "41E0006A412041011082808080001A200041003602840141C382C08000410D4108"
    //     //     "410720004184016A4104108080808000AC1081808080001A41D082C08000410720"
    //     //     "004184016A410441011082808080001A20004188016A41106A2201410036020020"
    //     //     "004188016A41086A22024200370300200042003703880141D782C0800041124109"
    //     //     "410820004188016A4114108080808000AC1081808080001A200041A0016A41106A"
    //     //     "2001280200360200200041A0016A41086A20022903003703002000200029038801"
    //     //     "3703A00141E982C08000410E200041A0016A411441011082808080001A20004200"
    //     //     "3703B80141F782C080004111410A4106200041B8016A41081080808080002201AC"
    //     //     "1081808080001A0240024020002D00B801220241A00171450D00418883C0800041"
    //     //     "13427F1081808080001A0C010B418883C08000411320002903B801220342018342"
    //     //     "388620034280FE0383422886842003428080FC0783421886200342808080F80F83"
    //     //     "4208868484200342088842808080F80F832003421888428080FC07838420034228"
    //     //     "884280FE038320034238888484842203420020037D200241C000711B1081808080"
    //     //     "001A0B02400240024002400240200141094F0D00419B83C08000410B200041B801"
    //     //     "6A200141011082808080001A02404130450D00200041C0016A41004130FC0B000B"
    //     //     "41F782C080004111410B4106200041C0016A41301080808080002201AC10818080"
    //     //     "80001A200141314F0D01024020010D0020004283808080703703F0010C040B0240"
    //     //     "024020002D00C0012204C02202417F4A0D0020014130460D012000428380808070"
    //     //     "3703F0010C050B02402002412071450D00024020014121470D00200041F0016A41"
    //     //     "196A200041D1016A290000370000200041F0016A41216A200041C0016A41196A29"
    //     //     "0000370000200020002900C90137008102200041023602F0012000200241C00171"
    //     //     "4106763A008002200020002900C101220342388620034280FE0383422886842003"
    //     //     "428080FC0783421886200342808080F80F834208868484200342088842808080F8"
    //     //     "0F832003421888428080FC07838420034228884280FE0383200342388884848437"
    //     //     "03F8010C060B20004283808080703703F0010C050B20014108460D032000428380"
    //     //     "8080703703F0010C040B200041F0016A412C6A200041C0016A41106A2903003702"
    //     //     "00200041A4026A200041C0016A41186A280200360200200041F0016A41186A2000"
    //     //     "41C0016A41246A29020037030020004190026A200041C0016A412C6A2802003602"
    //     //     "00200020002903C80137029402200020002902DC0137038002200020002903C001"
    //     //     "3703F801200041013602F00141A683C080004113200041F0016A41086A22024108"
    //     //     "41011082808080001A41B983C08000411E200241081083808080001A41D783C080"
    //     //     "00410B200041F0016A41106A411441011082808080001A41E283C08000410D2000"
    //     //     "41F0016A41246A411441011082808080001A200042003703A80202400240200241"
    //     //     "0841EF83C080004108200041A8026A4108410010848080800022024108470D0041"
    //     //     "F783C080004124200041A8026A410841011082808080001A41F783C08000412420"
    //     //     "0041A8026A41081083808080001A0C010B419B84C08000412D2002AC1081808080"
    //     //     "001A0B20002903F8012103410121020C040B20014108108680808000000B200141"
    //     //     "30108680808000000B200041003602F001200020002903C0012203420183423886"
    //     //     "20034280FE0383422886842003428080FC0783421886200342808080F80F834208"
    //     //     "868484200342088842808080F80F832003421888428080FC078384200342288842"
    //     //     "80FE038320034238888484842203420020037D200441C000711B3703F8010B41A6"
    //     //     "83C08000411341C884C08000410841011082808080001A410021020B419B83C080"
    //     //     "00410B200041C0016A200141011082808080001A024002402002450D0020002003"
    //     //     "3703A80241D084C08000410B200041A8026A410841011082808080001A0C010B41"
    //     //     "D084C08000410B41C884C08000410841011082808080001A0B200041B0026A2480"
    //     //     "8080800041000B0900108780808000000B0300000B0BE5040100418080C0000BDB"
    //     //     "0455494E54382056616C7565204C656E3A55494E54382056616C75653A55494E54"
    //     //     "38204865783A55494E5431362056616C7565204C656E3A55494E5431362056616C"
    //     //     "75653A55494E543136204865783A55494E5433322056616C7565204C656E3A5549"
    //     //     "4E5433322056616C75653A55494E543332204865783A55494E5436342056616C75"
    //     //     "65204C656E3A55494E5436342056616C75653A55494E543634204865783A55494E"
    //     //     "543132382056616C7565204C656E3A55494E543132382056616C75653A55494E54"
    //     //     "313238204865783A55494E543136302056616C7565204C656E3A55494E54313630"
    //     //     "2056616C75653A55494E54313630204865783A55494E543139322056616C756520"
    //     //     "4C656E3A55494E543139322056616C75653A55494E54313932204865783A55494E"
    //     //     "543235362056616C7565204C656E3A55494E54323536204865783A564C2056616C"
    //     //     "7565204C656E3A564C204865783A4143434F554E542056616C7565204C656E3A41"
    //     //     "43434F554E542056616C75653A414D4F554E542056616C7565204C656E3A414D4F"
    //     //     "554E542056616C75652028585250293A414D4F554E54204865783A414D4F554E54"
    //     //     "2056616C75652028494F55293A414D4F554E542056616C75652028494F5529202D"
    //     //     "204F726967696E616C3A494F55204973737565723A494F552043757272656E6379"
    //     //     "3AD4838D7EA4C68000414D4F554E542056616C75652028494F5529202D20416674"
    //     //     "657220616464696E6720313A4572726F7220616464696E6720464C4F41545F4F4E"
    //     //     "4520746F20494F5520616D6F756E742C20726573756C743A000000000000000049"
    //     //     "4F5520416D6F756E743A00B903046E616D6500100F706172616D65746572732E77"
    //     //     "61736D01FF020800365F5A4E387872706C5F73746434686F737431356F74786E5F"
    //     //     "63616C6C5F706172616D3137686264626330346266356630656331643845012F5F"
    //     //     "5A4E387872706C5F73746434686F73743974726163655F6E756D31376862396666"
    //     //     "61343664323065373166386345022B5F5A4E387872706C5F73746434686F737435"
    //     //     "7472616365313768376332613165636536303664316537664503395F5A4E387872"
    //     //     "706C5F73746434686F7374313874726163655F6F70617175655F666C6F61743137"
    //     //     "683733626632346130353361373561323945042F5F5A4E387872706C5F73746434"
    //     //     "686F737439666C6F61745F61646431376864323239336566303766363638313936"
    //     //     "45050463616C6C06425F5A4E34636F726535736C69636535696E6465783234736C"
    //     //     "6963655F656E645F696E6465785F6C656E5F6661696C3137686164666263376531"
    //     //     "61313539373461314507305F5A4E34636F72653970616E69636B696E673970616E"
    //     //     "69635F666D743137683431636665643739623264646266313345071201000F5F5F"
    //     //     "737461636B5F706F696E746572090A0100072E726F64617461004D0970726F6475"
    //     //     "6365727302086C616E6775616765010452757374000C70726F6365737365642D62"
    //     //     "79010572757374631D312E38382E30202836623030626333383820323032352D30"
    //     //     "362D3233290094010F7461726765745F6665617475726573082B0B62756C6B2D6D"
    //     //     "656D6F72792B0F62756C6B2D6D656D6F72792D6F70742B1663616C6C2D696E6469"
    //     //     "726563742D6F7665726C6F6E672B0A6D756C746976616C75652B0F6D757461626C"
    //     //     "652D676C6F62616C732B136E6F6E7472617070696E672D6670746F696E742B0F72"
    //     //     "65666572656E63652D74797065732B087369676E2D657874";

    //     env(contract::create(alice, contractWasmStr),
    //         contract::add_instance_param(
    //             tfSendAmount, "amount", "AMOUNT", XRP(2000)),
    //         contract::add_instance_param(0, "uint8", "UINT8", 1),
    //         contract::add_function(
    //             "call",
    //             {{0, "uint8", "UINT8"},
    //              {0, "uint16", "UINT16"},
    //              {0, "uint32", "UINT32"},
    //              {0, "uint64", "UINT64"},
    //              {0, "uint128", "UINT128"},
    //              {0, "uint160", "UINT160"},
    //              {0, "uint192", "UINT192"},
    //              {0, "uint256", "UINT256"},
    //              {0, "vl", "VL"},
    //              {0, "account", "ACCOUNT"},
    //              {0, "amountXRP", "AMOUNT"},
    //              {0, "amountIOU", "AMOUNT"},
    //              {0, "number", "NUMBER"}}),
    //         fee(XRP(200)),
    //         ter(tesSUCCESS));
    //     env.close();

    //     {
    //         Json::Value params;
    //         params[jss::ledger_index] = env.current()->seq() - 1;
    //         params[jss::transactions] = true;
    //         params[jss::expand] = true;
    //         auto const jrr = env.rpc("json", "ledger", to_string(params));
    //         std::cout << jrr << std::endl;
    //     }

    //     auto const contractAccount = getContractOwner(env);
    //     env(contract::call(alice, contractAccount, "call"),
    //         escrow::comp_allowance(1000000),
    //         contract::add_param(0, "uint8", "UINT8", 255),
    //         contract::add_param(0, "uint16", "UINT16", 65535),
    //         contract::add_param(
    //             0, "uint32", "UINT32", static_cast<std::uint32_t>(4294967295)),
    //         contract::add_param(0, "uint64", "UINT64", "9223372036854775807"),
    //         contract::add_param(
    //             0, "uint128", "UINT128", "00000000000000000000000000000001"),
    //         contract::add_param(
    //             0,
    //             "uint160",
    //             "UINT160",
    //             "0000000000000000000000000000000000000001"),
    //         contract::add_param(
    //             0,
    //             "uint192",
    //             "UINT192",
    //             "000000000000000000000000000000000000000000000001"),
    //         contract::add_param(
    //             0,
    //             "uint256",
    //             "UINT256",
    //             "D955DAC2E77519F05AD151A5D3C99FC8125FB39D58FF9F106F1ACA4491902C"
    //             "25"),
    //         contract::add_param(0, "vl", "VL", "DEADBEEF"),
    //         contract::add_param(0, "account", "ACCOUNT", alice.human()),
    //         contract::add_param(
    //             0,
    //             "amountXRP",
    //             "AMOUNT",
    //             XRP(1).value().getJson(JsonOptions::none)),
    //         contract::add_param(
    //             0,
    //             "amountIOU",
    //             "AMOUNT",
    //             USD(1.2).value().getJson(JsonOptions::none)),
    //         contract::add_param(0, "number", "NUMBER", "1.2"),
    //         ter(tesSUCCESS));
    //     env.close();

    //     {
    //         Json::Value params;
    //         params[jss::ledger_index] = env.current()->seq() - 1;
    //         params[jss::transactions] = true;
    //         params[jss::expand] = true;
    //         auto const jrr = env.rpc("json", "ledger", to_string(params));
    //         std::cout << jrr << std::endl;
    //     }
    // }

    void
    testEmitTxn(FeatureBitset features)
    {
        testcase("emit txn");

        using namespace jtx;

        test::jtx::Env env{*this, features};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        env.fund(XRP(10'000), alice, bob);
        env.close();

        std::string contractWasmStr = loadContractWasmStr("submit");

        auto const [contractAccount, contractHash, _] = submitContract(
            env,
            tesSUCCESS,
            contract::create(alice, contractWasmStr),
            contract::add_instance_param(
                tfSendAmount, "value", "AMOUNT", XRP(2000)),
            contract::add_function("emit", {}),
            fee(XRP(200)));

        env(contract::call(alice, contractAccount, "emit"),
            escrow::comp_allowance(1000000),
            ter(tesSUCCESS));
        env.close();
    }

    void
    testNestedEmitTxn(FeatureBitset features)
    {
        testcase("nested emit txn");

        using namespace jtx;

        test::jtx::Env env{*this, features};
        std::uint32_t const gasPrice = env.current()->fees().gasPrice;
        XRPAmount const feeDrops = env.current()->fees().base;

        auto const alice = Account{"alice"};
        env.fund(XRP(10'000), alice);
        env.close();

        auto const preAlice = env.balance(alice);

        // Create Contract #1
        auto const [contractAccount1, contractHash, jv] = submitContract(
            env,
            tesSUCCESS,
            contract::create(
                alice, loadContractWasmStr("submit_contract_call")),
            contract::add_instance_param(
                tfSendAmount, "value", "AMOUNT", XRP(2000)),
            contract::add_function(
                "emit",
                {{tfSendAmount, "value", "AMOUNT"},
                 {0, "account", "ACCOUNT"},
                 {0, "uint32", "UINT32"}}),
            fee(XRP(200)));

        // Create Contract #2
        auto const [contractAccount2, contractHash2, jv2] = submitContract(
            env,
            tesSUCCESS,
            contract::create(alice, loadContractWasmStr("submit")),
            contract::add_instance_param(
                tfSendAmount, "value", "AMOUNT", XRP(2000)),
            contract::add_function(
                "emit", {{0, "account", "ACCOUNT"}, {0, "uint32", "UINT32"}}),
            fee(XRP(200)));

        env(contract::call(alice, contractAccount1, "emit"),
            contract::add_param(tfSendAmount, "value", "AMOUNT", XRP(1)),
            contract::add_param(
                0, "account", "ACCOUNT", contractAccount2.human()),
            contract::add_param(0, "uint32", "UINT32", 100),
            escrow::comp_allowance(1'000'000),
            ter(tesSUCCESS));
        env.close();

        std::vector<TestLedgerData> testCases = {
            {0, "ContractCall", "tesSUCCESS"},
            {1, "ContractCall", "tesSUCCESS"},
            {2, "Payment", "tesSUCCESS"},
        };
        validateClosedLedger(env, testCases);
        auto const callFee = 1000000 * gasPrice / 1'000'000;
        BEAST_EXPECT(
            env.balance(alice) ==
            preAlice - XRP(200) - XRP(200) - XRP(2000) - XRP(2000) - XRP(1) -
                XRP(callFee) - feeDrops + drops(192));
        BEAST_EXPECT(env.balance(contractAccount1) == XRP(2000) + XRP(1));
        BEAST_EXPECT(env.balance(contractAccount2) == XRP(2000) - drops(192));
    }

    void
    testEvents(FeatureBitset features)
    {
        testcase("events");

        using namespace std::chrono_literals;
        using namespace jtx;

        test::jtx::Env env{*this, features};

        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        auto const gw = Account{"gw"};
        auto const USD = gw["USD"];
        env.fund(XRP(10'000), alice, bob);
        env.close();

        auto wsc = makeWSClient(env.app().config());
        Json::Value stream;

        {
            // RPC subscribe to contract events stream
            stream[jss::streams] = Json::arrayValue;
            stream[jss::streams].append("contract_events");
            auto jv = wsc->invoke("subscribe", stream);
            if (wsc->version() == 2)
            {
                BEAST_EXPECT(
                    jv.isMember(jss::jsonrpc) && jv[jss::jsonrpc] == "2.0");
                BEAST_EXPECT(
                    jv.isMember(jss::ripplerpc) && jv[jss::ripplerpc] == "2.0");
                BEAST_EXPECT(jv.isMember(jss::id) && jv[jss::id] == 5);
            }
            BEAST_EXPECT(jv[jss::result][jss::status] == "success");
        }

        std::string contractWasmStr = loadContractWasmStr("events");

        auto const [contractAccount, contractHash, _] = submitContract(
            env,
            tesSUCCESS,
            contract::create(alice, contractWasmStr),
            contract::add_instance_param(
                tfSendAmount, "amount", "AMOUNT", XRP(2000)),
            contract::add_function("events", {}),
            fee(XRP(200)));

        {
            Json::Value params;
            params[jss::ledger_index] = env.current()->seq() - 1;
            params[jss::transactions] = true;
            params[jss::expand] = true;
            auto const jrr = env.rpc("json", "ledger", to_string(params));
            std::cout << jrr << std::endl;
        }
        // {
        //     Json::Value params;
        //     params[jss::ledger_index] = env.current()->seq() - 1;
        //     params[jss::transactions] = true;
        //     params[jss::expand] = true;
        //     auto const jrr = env.rpc("json", "ledger", to_string(params));
        //     std::cout << jrr << std::endl;
        // }

        env(contract::call(alice, contractAccount, "events"),
            escrow::comp_allowance(1000000),
            ter(tesSUCCESS));
        env.close();

        {
            // Get contract info
            Json::Value params;
            params[jss::contract_account] = contractAccount.human();
            params[jss::account] = alice.human();
            auto const jrr =
                env.rpc("json", "contract_info", to_string(params));
            std::cout << jrr << std::endl;
        }

        // Check stream update
        BEAST_EXPECT(wsc->findMsg(5s, [&](auto const& jv) {
            return jv[jss::type] == "contractEvent" &&
                jv[jss::name] == "event1";
        }));

        // RPC unsubscribe
        auto jv = wsc->invoke("unsubscribe", stream);
        if (wsc->version() == 2)
        {
            BEAST_EXPECT(
                jv.isMember(jss::jsonrpc) && jv[jss::jsonrpc] == "2.0");
            BEAST_EXPECT(
                jv.isMember(jss::ripplerpc) && jv[jss::ripplerpc] == "2.0");
            BEAST_EXPECT(jv.isMember(jss::id) && jv[jss::id] == 5);
        }
        BEAST_EXPECT(jv[jss::status] == "success");
    }

    void
    testWithFeats(FeatureBitset features)
    {
        // testCreatePreflight(features);
        // testCreatePreclaim(features);
        // testCreateDoApply(features);
        // testModifyPreflight(features);
        // testModifyPreclaim(features);
        // testModifyDoApply(features);
        // testDeletePreflight(features);
        // testDeletePreclaim(features);
        // testDeleteDoApply(features);
        // testContractData(features);
        // testContractDataV2(features);
        // testContractDataAdvanced(features);
        // testParameters(features);
        // testEmitTxn(features);
        testNestedEmitTxn(features);
        // testEvents(features);
    }

public:
    void
    run() override
    {
        using namespace test::jtx;
        auto const sa = testable_amendments();
        testWithFeats(sa);
    }
};

BEAST_DEFINE_TESTSUITE(Contract, app, ripple);

}  // namespace test
}  // namespace ripple
