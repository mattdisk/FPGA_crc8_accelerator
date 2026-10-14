library IEEE;
use IEEE.STD_LOGIC_1164.ALL;

entity crc8_core_tb is
end crc8_core_tb;

architecture Behavioral of crc8_core_tb is

    signal clk     : std_logic := '0';
    signal reset   : std_logic := '0';
    signal init    : std_logic := '0';
    signal start   : std_logic := '0';
    signal data_in : std_logic_vector(7 downto 0) := (others => '0');

    signal busy    : std_logic;
    signal done    : std_logic;
    signal crc_out : std_logic_vector(7 downto 0);

begin

    uut : entity work.crc8_core
        port map (
            clk      => clk,
            reset    => reset,
            init     => init,
            start    => start,
            data_in  => data_in,
            busy     => busy,
            done     => done,
            crc_out  => crc_out
        );

    -- 100 MHz clock
    clk <= not clk after 5 ns;

    stimulus : process
    begin

        -- Reset
        reset <= '1';
        wait for 20 ns;
        reset <= '0';
        wait for 20 ns;

        -- Initialize CRC
        init <= '1';
        wait for 10 ns;
        init <= '0';
        wait for 10 ns;

        -- Process ASCII '1' = 0x31
        data_in <= x"31";
        start <= '1';
        wait for 10 ns;
        start <= '0';

        -- Wait for CRC calculation to finish
        wait until done = '1';
        wait for 20 ns;

        assert crc_out = x"97"
            report "CRC test FAILED"
            severity error;

        report "CRC test PASSED"
            severity note;

        wait;

    end process;

end Behavioral;